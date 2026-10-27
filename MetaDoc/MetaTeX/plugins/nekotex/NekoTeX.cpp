#include "MetaTeX.h"
#include <string>

using namespace metatex;
namespace {
void beginNote(Engine& e,const std::string& role,const std::string& title){
    e.beginBlockContext(BlockKind::Note,role);
    if(!title.empty())e.addText(title+": ");
}
void note(Engine& e,const std::string& role,const std::string& fallback){
    auto title=e.readOptionalText(fallback);auto body=e.readArgument();
    beginNote(e,role,title);e.executeGrouped(body);e.endBlockContext();
}
std::string exerciseTitle(Engine& e){
    auto title=e.readOptionalText();e.stepCounter("exercise");
    auto number=std::to_string(e.counter("exercise"));e.setReferenceValue(number);
    return "Exercise "+number+(title.empty()?"":" ("+title+")");
}
}
extern "C" const char* metatex_plugin_id(){return "nekotex";}
extern "C" void metatex_register_plugin(Engine* engine){
    Engine& e=*engine;
    e.registerCommand("nekotex",[](Engine& x){x.addText("NekoTeX");});
    for(auto [role,title]:{std::pair{"note","Note"},{"tip","Tip"},{"warning","Warning"},{"important","Important"},{"solution","Solution"},{"summary","Summary"}}){
        e.registerCommand(role,[role,title](Engine& x){note(x,role,title);});
        e.registerEnvironment(role,[role,title](Engine& x){beginNote(x,role,x.readOptionalText(title));},[](Engine& x){x.endBlockContext();});
    }
    e.registerCommand("callout",[](Engine& x){note(x,"information","Information");});
    e.registerEnvironment("callout",[](Engine& x){beginNote(x,"information",x.readOptionalText("Information"));},[](Engine& x){x.endBlockContext();});
    e.registerCommand("codeblock",[](Engine& x){auto language=x.readOptionalText();auto text=x.readLiteralArgument();x.codeBlock(std::move(text),std::move(language));});
    // A required language group leaves the raw source cursor exactly at the body.
    e.registerEnvironment("codeblock",[](Engine& x){
        auto language=x.readTextArgument();auto text=x.readLiteralEnvironment("codeblock");
        x.codeBlock(std::move(text),std::move(language));x.endEnvironment("codeblock");
    });
    e.registerCommand("codefile",[](Engine& x){
        auto language=x.readOptionalText();auto name=x.readTextArgument();
        try{
            std::filesystem::path path=name;
            if(path.is_relative()&&x.currentFile()!="<memory>")path=x.currentFile().parent_path()/path;
            auto text=readFile(path,4*1024*1024);Lexer validate(text);
            x.codeBlock(std::move(text),std::move(language));
        }catch(const std::exception& ex){x.report(std::string("codefile: ")+ex.what(),true);}
    });
    e.registerCommand("component",[](Engine& x){auto name=x.readTextArgument();auto body=x.readArgument();beginNote(x,"component",name);x.executeGrouped(body);x.endBlockContext();});
    e.registerEnvironment("component",[](Engine& x){beginNote(x,"component",x.readTextArgument());},[](Engine& x){x.endBlockContext();});
    e.registerCommand("exercise",[](Engine& x){auto title=exerciseTitle(x);auto body=x.readArgument();beginNote(x,"exercise",title);x.executeGrouped(body);x.endBlockContext();});
    e.registerEnvironment("exercise",[](Engine& x){beginNote(x,"exercise",exerciseTitle(x));},[](Engine& x){x.endBlockContext();});
    e.registerCommand("concept",[](Engine& x){x.styledArgument(SpanKind::Bold);});
    e.registerCommand("kbd",[](Engine& x){x.addSpan(SpanKind::Code,x.readLiteralArgument());});
    e.registerCommand("inlinecode",[](Engine& x){x.addSpan(SpanKind::Code,x.readLiteralArgument());});
    e.registerCommand("meta",[](Engine& x){x.styledArgument(SpanKind::SmallCaps);});
    e.registerCommand("separator",[](Engine& x){x.startBlock(BlockKind::HorizontalRule);x.paragraph();});
}
