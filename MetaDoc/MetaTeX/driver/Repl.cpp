#include "Repl.h"
#include "Driver.hpp"
#include <replxx.hxx>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace metatex {
namespace {
bool diagnostics(const Document& doc){
    bool error=false;
    for(const auto& d:doc.diagnostics){
        bool failed=d.severity==Diagnostic::Severity::Error;error|=failed;
        std::cerr<<d.file<<':'<<d.line<<':'<<d.column<<": "<<(failed?"error: ":"warning: ")<<d.message<<'\n';
    }return error;
}
void save(const Document& doc,const std::string& output,const std::string& target){
    writeFile(output,renderDevice(doc,target));
}
}
int runRepl(Engine& engine,const std::string& format,const std::string& input,const std::string& output,const std::string& target){
    const bool terminal=::isatty(STDIN_FILENO)&&::isatty(STDOUT_FILENO);
    const std::string name=format=="plain"?"mtex":format=="latex"?"mlatex":"nlatex";
    Engine baseline=engine;Document document;
    replxx::Replxx editor;
    editor.set_max_history_size(1000);
    editor.set_completion_callback([&](const std::string& line,int& context){
        replxx::Replxx::completions_t completions;
        auto at=line.find_last_of(" \\{}\t");auto prefix=line.substr(at==std::string::npos?0:at+1);
        context=static_cast<int>(prefix.size());
        auto names=engine.commandNames();
        names.insert(names.end(),{":help",":quit",":reset",":show",":write",":load",":begin",":end",":abort"});
        for(const auto& candidate:names)if(candidate.starts_with(prefix))completions.emplace_back(candidate);
        return completions;
    });
    std::filesystem::path history;
    if(terminal){
        if(const char* state=std::getenv("XDG_STATE_HOME");state&&*state)history=state;
        else if(const char* home=std::getenv("HOME");home&&*home)history=std::filesystem::path(home)/".local/state";
        if(!history.empty()){
            history/="metatex";std::error_code ec;std::filesystem::create_directories(history,ec);
            if(ec)history.clear();else {history/=name+".history";editor.history_load(history.c_str());}
        }
        std::cout<<name<<" — MetaTeX REPL (:help for commands, Ctrl-D to exit)\n";
    }
    int status=0;
    auto evaluate=[&](std::string source,const std::filesystem::path& filename){
        try {
            auto fragment=engine.compileFragment(std::move(source),filename);
            if(diagnostics(fragment)){status=1;return;}
            std::cout<<renderText(fragment)<<std::flush;
            document.blocks.insert(document.blocks.end(),fragment.blocks.begin(),fragment.blocks.end());
            document.title=fragment.title;document.author=fragment.author;document.date=fragment.date;
            document.references=std::move(fragment.references);
        }catch(const std::exception& ex){std::cerr<<"metatex: "<<ex.what()<<'\n';status=1;}
    };
    if(!input.empty()){
        try {evaluate(readFile(input),std::filesystem::absolute(input));}
        catch(const std::exception& ex){std::cerr<<"metatex: "<<ex.what()<<'\n';status=1;}
    }
    std::string pending;bool explicitBlock=false;
    while(true){
        errno=0;const char* next=editor.input(terminal?(pending.empty()?(name+"> ").c_str():"...> "):"");
        if(!next){if(errno==EAGAIN)continue;break;}
        std::string line=next;
        if(terminal&&!line.empty())editor.history_add(line);
        if(line==":quit")break;
        if(line==":abort"){pending.clear();explicitBlock=false;continue;}
        if(line==":begin"&&pending.empty()){explicitBlock=true;continue;}
        if(line==":end"&&explicitBlock){explicitBlock=false;evaluate(pending,"<repl>");pending.clear();continue;}
        if(pending.empty()&&!explicitBlock&&line.starts_with(':')){
            try {
                if(line==":help")std::cout<<":help, :quit, :reset, :show, :load FILE, :write FILE\n"
                    <<":begin / :end collect a block; :abort discards pending input.\n"
                    <<"Definitions persist; each submission executes once. :write uses the CLI target.\n";
                else if(line==":reset"){engine=baseline;document={};}
                else if(line==":show")std::cout<<renderText(document);
                else if(line.starts_with(":load ")){auto file=line.substr(6);evaluate(readFile(file),std::filesystem::absolute(file));}
                else if(line.starts_with(":write ")){
                    auto file=line.substr(7);
                    driver::checkOutputPath(input,file);
                    save(document,file,target);
                }else throw std::runtime_error("unknown REPL command: "+line);
            }catch(const std::exception& ex){std::cerr<<"metatex: "<<ex.what()<<'\n';status=1;}
            continue;
        }
        if(pending.size()+line.size()+1>16*1024*1024){std::cerr<<"metatex: REPL input limit exceeded\n";pending.clear();explicitBlock=false;status=1;continue;}
        pending+=line+'\n';
        try {if(explicitBlock||driver::incompleteSource(pending))continue;}
        catch(const std::exception& ex){std::cerr<<"metatex: "<<ex.what()<<'\n';pending.clear();status=1;continue;}
        evaluate(pending,"<repl>");pending.clear();
    }
    if(!pending.empty()||explicitBlock){std::cerr<<"metatex: incomplete REPL input; use :abort to discard it\n";status=1;}
    if(!history.empty())editor.history_save(history.c_str());
    if(!output.empty()&&status==0)save(document,output,target);
    return status;
}
}
