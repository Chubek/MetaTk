#include "MetaTeX.h"
#include "Repl.h"
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
namespace {
void usage(){std::cout<<"MetaTeX 0.1 (working subset)\n"
    "Usage: metatex [--format plain|latex|nekotex] [--target html|pdf|text|json] [-o OUTPUT] INPUT\n"
    "       --plugin NAME (repeatable; use metavect for vector graphics)\n"
    "       -i, --interactive: REPL, optionally after reading INPUT\n"
    "       metatex --help | --version | --list-devices\n"
    "Reads a local UTF-8 TeX source file. Diagnostics are printed to stderr.\n";}
struct PluginLibraries {
    std::vector<std::unique_ptr<dynalo::library>> handles;
    std::vector<std::string> names;
    ~PluginLibraries(){while(!handles.empty())handles.pop_back();}
};
void loadPlugin(const std::string& name,const fs::path& exe,PluginLibraries& libraries,Engine& engine){
    if(name.empty()||!std::all_of(name.begin(),name.end(),[](unsigned char c){return std::isalnum(c)||c=='_'||c=='-';}))
        throw std::runtime_error("invalid plugin name: "+name);
    if(std::find(libraries.names.begin(),libraries.names.end(),name)!=libraries.names.end())return;
    std::vector<fs::path> paths;
    const std::string file="metatex_"+name+METATEX_PLUGIN_SUFFIX;
    paths.push_back(exe.parent_path()/file);
    paths.push_back(exe.parent_path().parent_path()/"lib"/"metatex"/file);
    paths.push_back(fs::current_path()/file);
    if(const char* env=std::getenv("METATEX_PLUGIN_PATH")){
        std::string list=env;std::size_t at=0;
        while(at<list.size()){auto end=list.find(':',at);if(end==std::string::npos)end=list.size();
            if(end>at)paths.push_back(fs::path(list.substr(at,end-at))/file);
            at=end+1;
        }
    }
    std::string errors;
    for(const auto& path:paths){
        std::unique_ptr<dynalo::library> h;
        try {h=std::make_unique<dynalo::library>(path.string());}
        catch(const std::exception& ex){errors+=path.string()+": "+ex.what()+"\n";continue;}
        auto id=h->get_function<const char*()>("metatex_plugin_id");
        auto initialize=h->get_function<void(Engine*)>("metatex_register_plugin");
        const char* pluginId=id?id():nullptr;
        if(!pluginId||!initialize||name!=pluginId)throw std::runtime_error("incompatible plugin: "+path.string());
        libraries.handles.push_back(std::move(h));libraries.names.push_back(name);initialize(&engine);return;
    }
    throw std::runtime_error("unable to load "+name+" plugin; build/install it with MetaTeX:\n"+errors);
}
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
            if(a=="--list-devices"){std::cout<<"html\npdf\ntext\njson\n";return 0;}
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
        if(target!="pdf"&&target!="html"&&target!="text"&&target!="json")throw std::runtime_error("unsupported device: "+target);
        if(output.empty()&&!interactive)output=fs::path(input).replace_extension(target=="text"?"txt":target).string();
        if(!input.empty()&&!output.empty()&&(fs::weakly_canonical(input)==fs::weakly_canonical(output) ||
           (fs::exists(output)&&fs::equivalent(input,output))))throw std::runtime_error("output must not overwrite the input file");
        // Libraries outlive the Engine's std::function handlers, including on exceptions.
        PluginLibraries libraries;
        Engine engine;
        fs::path executable=fs::absolute(argv[0]);
#ifdef __linux__
        std::error_code ec;auto actual=fs::read_symlink("/proc/self/exe",ec);if(!ec)executable=actual;
#endif
        if(format=="latex"||format=="nekotex")loadPlugin("latex",executable,libraries,engine);
        if(format=="nekotex")loadPlugin("nekotex",executable,libraries,engine);
        for(const auto& plugin:plugins)loadPlugin(plugin,executable,libraries,engine);
        if(interactive)return runRepl(engine,format,input,output,target);
        auto doc=engine.compile(readFile(input),input);
        std::size_t errors=0;
        for(const auto& d:doc.diagnostics){
            if(d.severity==Diagnostic::Severity::Error)++errors;
            std::cerr<<d.file<<":"<<d.line<<":"<<d.column<<": "
                     <<(d.severity==Diagnostic::Severity::Error?"error: ":"warning: ")<<d.message<<"\n";
        }
        if(errors){std::cerr<<errors<<" error(s); output was not written\n";return 1;}
        if(target=="pdf") {
            if(!renderCairoPDF(doc,output))writeFile(output,renderPDF(doc));
        } else writeFile(output,target=="html"?renderHTML(doc):target=="json"?renderJSON(doc):renderText(doc));
        std::cout<<"Wrote "<<output<<" ("<<doc.blocks.size()<<" blocks)\n";
        return 0;
    }catch(const std::exception& ex){std::cerr<<"metatex: "<<ex.what()<<"\n";return 2;}
}
