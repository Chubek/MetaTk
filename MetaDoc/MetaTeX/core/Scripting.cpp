#include "Scripting.h"
#include <charconv>
#include <stdexcept>

namespace metatex {
namespace {
int integer(std::string_view text,int low,int high){
    int value=0;auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size()||value<low||value>high)
        throw std::runtime_error("integer must be in ["+std::to_string(low)+", "+std::to_string(high)+"]");
    return value;
}
detail::ScriptOptions options(std::string_view text){
    detail::ScriptOptions result;
    while(!text.empty()){
        auto comma=text.find(',');auto item=text.substr(0,comma);
        if(item.starts_with("fd="))result.descriptor=integer(item.substr(3),1,255);
        else if(item.starts_with("timeout="))result.timeout=std::chrono::milliseconds(integer(item.substr(8),1,600000));
        else throw std::runtime_error("expected fd=N or timeout=MILLISECONDS");
        if(comma==std::string_view::npos)break;
        text.remove_prefix(comma+1);
        if(text.empty())throw std::runtime_error("empty external-command option");
    }
    return result;
}
std::filesystem::path relativeTo(Engine& engine,const std::string& path){
    if(path.empty())throw std::runtime_error("empty script filename");
    std::filesystem::path result=path;
    if(result.is_relative())result=engine.currentFile().parent_path()/result;
    return std::filesystem::absolute(result);
}
std::string commandName(Engine& engine){
    auto tokens=engine.readArgument();
    if(tokens.size()!=1||tokens[0].kind!=TokenKind::Control||tokens[0].value.empty())
        throw std::runtime_error("expected a control sequence, e.g. {\\greet}");
    return tokens[0].value;
}
void definition(Engine& engine,bool external,bool file){
    try {
        auto before=engine.document().diagnostics.size();
        auto name=commandName(engine);
        int arity=integer(engine.readOptionalText("0"),0,9);
        auto config=external?options(engine.readOptionalText()):detail::ScriptOptions{};
        auto body=(file||external)?engine.readTextArgument():engine.tokensText(engine.readArgument());
        if(engine.document().diagnostics.size()!=before)return;
        auto location=engine.currentFile();
        std::filesystem::path path;
        if(file||external)path=relativeTo(engine,body);
        if(file){body=readFile(path);location=path;}
        engine.defineCommand(name,[arity,config,body,location,path,external](Engine& e){
            std::vector<std::string> arguments;
            auto before=e.document().diagnostics.size();
            for(int i=0;i<arity;++i)arguments.push_back(e.tokensText(e.readArgument()));
            if(e.document().diagnostics.size()!=before)return;
            try {
                auto output=external?detail::runExternal(path,arguments,config):detail::runMetaML(body,arguments,location);
                e.insert(Lexer::scan(output));
            }catch(const std::exception& ex){e.report(std::string(external?"Extern: ":"MetaML: ")+ex.what(),true);}
        });
    }catch(const std::exception& ex){engine.report(ex.what(),true);}
}
}
void Engine::installScripting(){
    registerCommand("mmlcommand",[](Engine& e){definition(e,false,false);});
    registerCommand("mmlcommandfile",[](Engine& e){definition(e,false,true);});
    registerCommand("externcommand",[](Engine& e){definition(e,true,false);});
    registerCommand("Extern",[](Engine& e){
        auto before=e.document().diagnostics.size();
        auto settings=e.readOptionalText();auto filename=e.readTextArgument();
        if(e.document().diagnostics.size()!=before)return;
        try {e.insert(Lexer::scan(detail::runExternal(relativeTo(e,filename),{},options(settings))));}
        catch(const std::exception& ex){e.report(std::string("Extern: ")+ex.what(),true);}
    });
}
}
