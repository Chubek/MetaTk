#include "Scripting.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;

namespace metatex::detail {
namespace {
struct FD {
    int value=-1;
    explicit FD(int n=-1):value(n){}
    ~FD(){if(value>=0)::close(value);}
    void close(){if(value>=0)::close(value);value=-1;}
    FD(const FD&)=delete;
    FD& operator=(const FD&)=delete;
};
void check(int error,const char* operation){
    if(error)throw std::system_error(error,std::generic_category(),operation);
}
struct Spawn {
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    Spawn(){
        check(posix_spawn_file_actions_init(&actions),"spawn actions");
        int error=posix_spawnattr_init(&attributes);
        if(error){posix_spawn_file_actions_destroy(&actions);check(error,"spawn attributes");}
    }
    ~Spawn(){posix_spawn_file_actions_destroy(&actions);posix_spawnattr_destroy(&attributes);}
};
struct Child {
    pid_t pid=-1;
    bool reaped=false;
    bool complete=false;
    ~Child(){
        if(pid<=0)return;
        if(!complete)::kill(-pid,SIGKILL);
        if(!reaped){int status;while(::waitpid(pid,&status,0)<0&&errno==EINTR){}}
    }
};
}
std::string runExternal(const std::filesystem::path& script,const std::vector<std::string>& arguments,const ScriptOptions& options){
    if(options.descriptor<1||options.descriptor>255)throw std::runtime_error("capture descriptor must be in [1, 255]");
    auto path=script.string();
    if(path.find('\0')!=std::string::npos)throw std::runtime_error("NUL in script filename");
    for(const auto& arg:arguments)if(arg.find('\0')!=std::string::npos)throw std::runtime_error("NUL in script argument");
    std::ifstream file(script,std::ios::binary);char shebang[2]{};
    if(!file.read(shebang,2)||shebang[0]!='#'||shebang[1]!='!')
        throw std::runtime_error("script must be readable and start with a shebang: "+path);
    file.close();
    int pipefd[2];
#ifdef __linux__
    if(::pipe2(pipefd,O_CLOEXEC)<0)check(errno,"capture pipe");
#else
    if(::pipe(pipefd)<0)check(errno,"capture pipe");
#endif
    FD reader(pipefd[0]),writer(pipefd[1]);
    // Move both endpoints above selectable descriptors; dup2 must clear CLOEXEC
    // on the child target even when its requested fd happened to be a pipe fd.
    const int firstFree=std::max(3,options.descriptor+1);
    FD readEnd(::fcntl(reader.value,F_DUPFD_CLOEXEC,firstFree));
    if(readEnd.value<0)check(errno,"capture read descriptor");
    FD writeEnd(::fcntl(writer.value,F_DUPFD_CLOEXEC,firstFree));
    if(writeEnd.value<0)check(errno,"capture write descriptor");
    reader.close();writer.close();
    int flags=::fcntl(readEnd.value,F_GETFL);
    if(flags<0||::fcntl(readEnd.value,F_SETFL,flags|O_NONBLOCK)<0)check(errno,"nonblocking capture");
    Spawn spawn;
    check(posix_spawnattr_setflags(&spawn.attributes,POSIX_SPAWN_SETPGROUP),"spawn flags");
    check(posix_spawnattr_setpgroup(&spawn.attributes,0),"spawn process group");
    check(posix_spawn_file_actions_addopen(&spawn.actions,STDIN_FILENO,"/dev/null",O_RDONLY,0),"script stdin");
    if(options.descriptor!=STDOUT_FILENO)
        check(posix_spawn_file_actions_adddup2(&spawn.actions,STDERR_FILENO,STDOUT_FILENO),"script log output");
    check(posix_spawn_file_actions_adddup2(&spawn.actions,writeEnd.value,options.descriptor),"script output");
    check(posix_spawn_file_actions_addclose(&spawn.actions,readEnd.value),"close capture reader");
    check(posix_spawn_file_actions_addclose(&spawn.actions,writeEnd.value),"close capture writer");
    std::vector<char*> argv{path.data()};
    for(const auto& arg:arguments)argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    Child child;
    int error=posix_spawn(&child.pid,path.c_str(),&spawn.actions,&spawn.attributes,argv.data(),environ);
    if(error){child.pid=-1;check(error,("execute "+path).c_str());}
    writeEnd.close();
    const auto deadline=std::chrono::steady_clock::now()+options.timeout;
    std::string output;int status=0;bool eof=false;
    while(!child.reaped||!eof){
        if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("script timed out: "+path);
        if(!eof){
            char bytes[8192];
            // Bound each drain to let the deadline run even for a continuous writer.
            for(int i=0;i<32;++i){
                ssize_t n=::read(readEnd.value,bytes,sizeof(bytes));
                if(n>0){
                    if(static_cast<std::size_t>(n)>options.outputLimit-output.size())throw std::runtime_error("script output limit exceeded");
                    output.append(bytes,static_cast<std::size_t>(n));
                }else if(n==0){eof=true;break;}
                else if(errno==EINTR)continue;
                else if(errno==EAGAIN||errno==EWOULDBLOCK)break;
                else check(errno,"read script output");
            }
        }
        if(!child.reaped){
            pid_t state=::waitpid(child.pid,&status,WNOHANG);
            if(state==child.pid)child.reaped=true;
            else if(state<0&&errno!=EINTR)check(errno,"wait for script");
        }
        if(child.reaped&&eof)break;
        pollfd fd{readEnd.value,POLLIN,0};
        int ready=::poll(eof?nullptr:&fd,eof?0:1,10);
        if(ready<0&&errno!=EINTR)check(errno,"poll script output");
    }
    if(!WIFEXITED(status)||WEXITSTATUS(status)!=0)
        throw std::runtime_error(WIFEXITED(status)?"script exited with status "+std::to_string(WEXITSTATUS(status)):"script terminated by signal "+std::to_string(WTERMSIG(status)));
    child.complete=true;
    return output;
}
}
