#include "MetaTeX.h"
#include "Repl.h"
#include "Driver.hpp"
#include "../sink/DiagSinks.hpp"
#include <dynalo/dynalo.hpp>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using namespace metatex;
using namespace metatex::driver;
namespace {
void usage(){std::cout<<"MetaTeX 0.1 (working subset)\n"
    "Usage: metatex [--format plain|latex|nekotex] [--target html|pdf|text|json|ps|troff-ir|epub] [-o OUTPUT] INPUT\n"
    "       --plugin NAME (repeatable; use metavect for vector graphics)\n"
    "       -i, --interactive: REPL, optionally after reading INPUT\n"
    "       metatex --help | --version | --list-devices\n"
    "Reads a local UTF-8 TeX source file. Diagnostics are printed to stderr.\n";}
}
int main(int argc,char**argv){
    try {
        std::string input,output,format,target;
        std::vector<std::string> plugins;
        bool interactive=false;
        for(int i=1;i<argc;i++){
            std::string a=argv[i];
            if(a=="--help"||a=="-h"){usage();return 0;}
            if(a=="--version"){std::cout<<"MetaTeX 0.1.0\n";return 0;}
            if(a=="--list-devices"){for(const auto& device:devices())std::cout<<device.name<<'\n';return 0;}
            if(a=="-i"||a=="--interactive"){interactive=true;continue;}
            if(a=="-o"||a=="--output"){if(++i>=argc)throw std::runtime_error("missing output argument");output=argv[i];continue;}
            if(a=="--format"||a=="-f"){if(++i>=argc)throw std::runtime_error("missing format argument");format=argv[i];continue;}
            if(a=="--target"||a=="-T"){if(++i>=argc)throw std::runtime_error("missing target argument");target=argv[i];continue;}
            if(a=="--plugin"){if(++i>=argc)throw std::runtime_error("missing plugin argument");plugins.emplace_back(argv[i]);continue;}
            if(a.rfind("--plugin=",0)==0){plugins.push_back(a.substr(9));continue;}
            if(a.rfind("--format=",0)==0){format=a.substr(9);continue;}
            if(a.rfind("--target=",0)==0){target=a.substr(9);continue;}
            if(!a.empty() && a[0]=='-')throw std::runtime_error("unknown option: "+a);
            if(!input.empty())throw std::runtime_error("only one input file is accepted");
            input=a;
        }
        if(input.empty())interactive=true;
        if(format.empty())format=fs::path(input).extension()==".nltx"?"nekotex":"latex";
        if(target.empty())target=interactive?"text":"pdf";
        if(format!="plain"&&format!="latex"&&format!="nekotex")throw std::runtime_error("unsupported format: "+format);
        (void)findDevice(target);
        if(output.empty()&&!interactive)output=defaultOutput(input,target).string();
        checkOutputPath(input,output);
        // Libraries outlive the Engine's std::function handlers, including on exceptions.
        PluginLibraries libraries;
        Engine engine;
        fs::path executable=fs::absolute(argv[0]);
#ifdef __linux__
        std::error_code ec;auto actual=fs::read_symlink("/proc/self/exe",ec);if(!ec)executable=actual;
#endif
        if(format=="latex")loadLaTeX(executable,libraries,engine);
        if(format=="nekotex")loadNekoTeX(executable,libraries,engine);
        for(const auto& plugin:plugins)loadPlugin(plugin,executable,libraries,engine);
        if(interactive)return runRepl(engine,format,input,output,target);
        auto doc=engine.compile(readFile(input),input);
        io::DescriptorSink diagnosticOutput(2);io::DiagnosticSink diagnostics(diagnosticOutput);
        auto errors=diagnostics.write(doc);
        if(errors){std::cerr<<errors<<" error(s); output was not written\n";return 1;}
        writeFile(output,renderDevice(doc,target));
        std::cout<<"Wrote "<<output<<" ("<<doc.blocks.size()<<" blocks)\n";
        return 0;
    }catch(const std::exception& ex){std::cerr<<"metatex: "<<ex.what()<<"\n";return 2;}
}
