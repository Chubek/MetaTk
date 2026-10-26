#include "Driver.hpp"
#include <iostream>
#include <sstream>
#include <vector>

namespace gppx::driver {
namespace {
std::string trim(std::string s) {
    auto a=s.find_first_not_of(" \t\r\n");
    if (a==s.npos) return {};
    return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
void help() {
    std::cerr << R"(GPPX REPL commands:
 :help                This help
 :quit / :q           Exit
 :define NAME VALUE   Define object macro (VALUE may contain spaces)
 :undef NAME          Remove object macro
 :syntax KEY VALUE    Change syntax or option
 :profile NAME        Switch profile (c, percent, template)
 :plugin PATH         Load shared plugin or plugin directory
 :config FILE         Apply a config file
 :file FILE           Process an input file
 :match ENGINE P S    Match pattern P against subject S (no spaces)
 :begin               Capture multiple lines until :end
 :end                 Finish multiline input
Input otherwise processes one line and retains macros between turns.
)";
}
void printResult(Engine& e,const std::string& code,const std::string& source) {
    auto result=e.process(code,source);
    std::cout.write(result.data(),static_cast<std::streamsize>(result.size()));
    std::cout.flush();
    for (auto& d:e.diagnostics())
        std::cerr<<d.path<<':'<<d.line<<": "<<(d.warning?"warning: ":"error: ")<<d.message<<'\n';
}
}
int repl(Engine& engine) {
    help();
    bool recording=false;
    std::string line,buffer;
    while (std::cerr<<(recording?"... ":"gppx> ") && std::getline(std::cin,line)) {
        try {
            if (recording) {
                if (line==":end") {recording=false;printResult(engine,buffer,"<repl-block>");buffer.clear();}
                else {buffer+=line;buffer+='\n';}
                continue;
            }
            if (line==":quit"||line==":q") return 0;
            if (line==":help") {help();continue;}
            if (line==":begin") {recording=true;buffer.clear();continue;}
            if (line.rfind(":",0)==0) {
                auto pos=line.find(' ');
                auto cmd=line.substr(1,pos==line.npos?line.npos:pos-1);
                auto args=pos==line.npos?std::string():trim(line.substr(pos+1));
                if (cmd=="file") {if (args.empty()) throw Error("expected FILE");std::cout<<engine.processFile(args);}
                else if (cmd=="plugin") {if (args.empty()) throw Error("expected PATH");plugins(engine,{args});}
                else if (cmd=="config") {if (args.empty()) throw Error("expected FILE");config(engine,args);}
                else if (cmd=="undef") {if (args.empty()) throw Error("expected NAME");engine.undef(args);}
                else if (cmd=="profile") {if (args.empty()) throw Error("expected NAME");engine.set("profile",args);}
                else if (cmd=="define"||cmd=="syntax") {
                    auto sp=args.find_first_of(" \t");
                    if (sp==args.npos) throw Error("expected KEY VALUE");
                    auto key=args.substr(0,sp),value=trim(args.substr(sp+1));
                    if (cmd=="define") engine.define(key,value);else engine.set(key,value);
                } else if (cmd=="match") {
                    std::istringstream in(args);std::string kind,pat,sub;
                    if (!(in>>kind>>pat>>sub)) throw Error("usage: :match ENGINE PATTERN SUBJECT");
                    std::cerr<<(engine.match(kind,pat,sub)?"match":"no match")<<'\n';
                } else throw Error("unknown REPL command: "+cmd);
            } else printResult(engine,line+"\n","<repl>");
        } catch (const std::exception& ex) {std::cerr<<"error: "<<ex.what()<<'\n';}
    }
    if (recording) std::cerr<<"warning: discarded unterminated :begin block\n";
    return recording?1:0;
}
}
