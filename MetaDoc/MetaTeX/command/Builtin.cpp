#include "Builtin.hpp"
#include "Symbol.hpp"
#include <algorithm>
#include <ctime>
namespace metatex::command {
const std::vector<std::string>& builtinCommandNames() {
    static const auto names = Engine{}.commandNames(); return names;
}
bool isBuiltin(std::string_view name) {
    const auto& names = builtinCommandNames(); return std::binary_search(names.begin(), names.end(), name);
}
}
namespace metatex {
void Engine::installPrimitives(){
    registerCommand("def",[](Engine& e){e.scanDefinition(false);});
    registerCommand("gdef",[](Engine& e){e.scanDefinition(true);});
    registerCommand("edef",[](Engine& e){e.scanDefinition(false,true);});
    registerCommand("xdef",[](Engine& e){e.scanDefinition(true,true);});
    registerCommand("let",[](Engine& e){Token dst=e.pop(true);if(dst.kind!=TokenKind::Control){e.report("\\let expects control sequence",true);return;}
        if(e.peek(true).value=="=")e.pop();
        Token src=e.pop(true);
        if(src.kind==TokenKind::Control){
            if(const Macro* m=e.lookup(src.controlName())){e.defineMacro(dst.controlName(),*m,e.globalAssignment_);return;}
            if(auto reg=e.registerAlias(src.controlName())){e.defineRegister(dst.controlName(),*reg);return;}
        }
        e.defineMacro(dst.controlName(),{0,{src},{},{}},e.globalAssignment_);
    });
    registerCommand("input",[](Engine& e){e.readInput();});
    registerCommand("endinput",[](Engine& e){e.endInput_=true;});
    registerCommand("relax",[](Engine&){});
    registerCommand("par",[](Engine& e){e.paragraph();});
    registerCommand("hskip",[](Engine& e){e.addSpace();e.readArgument();});
    registerCommand("noindent",[](Engine&){});
    registerCommand("bye",[](Engine& e){e.stopped_=true;});
    registerCommand("bgroup",[](Engine& e){e.beginGroup();});
    registerCommand("egroup",[](Engine& e){e.endGroup();});
    registerCommand("aftergroup",[](Engine& e){
        auto token=e.pop();
        if(token.kind==TokenKind::Sentinel){e.report("missing token after \\aftergroup",true);return;}
        if(!e.afterGroups_.empty())e.afterGroups_.back().push_back(std::move(token));
    });
    registerCommand("endcsname",[](Engine& e){e.report("unexpected \\endcsname",true);});
    registerCommand("message",[](Engine& e){e.report("TeX message: "+e.readTextArgument());});
    registerCommand("errmessage",[](Engine& e){e.report(e.readTextArgument(),true);});
    command::installTextSymbols(*this);
    registerCommand("today",[](Engine& e){
        const auto now=std::time(nullptr); std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm,&now);
#else
        localtime_r(&now,&tm);
#endif
        char buf[32]{};std::strftime(buf,sizeof(buf),"%Y-%m-%d",&tm);e.addText(buf);
    });
    registerCommand("!",[](Engine&){});
    registerCommand("(",[](Engine& e){e.math(false,")");});
    registerCommand("[",[](Engine& e){e.math(true,"]");});
    registerCommand("textbf",[](Engine& e){e.styledArgument(SpanKind::Bold);});
    registerCommand("textit",[](Engine& e){e.styledArgument(SpanKind::Italic);});
    registerCommand("emph",[](Engine& e){e.styledArgument(SpanKind::Italic);});
    registerCommand("texttt",[](Engine& e){e.styledArgument(SpanKind::Code);});
    registerCommand("textsc",[](Engine& e){e.styledArgument(SpanKind::SmallCaps);});
    registerCommand("underline",[](Engine& e){e.styledArgument(SpanKind::Underline);});
    registerCommand("bfseries",[](Engine& e){e.styles_={SpanKind::Bold};});
    registerCommand("itshape",[](Engine& e){e.styles_={SpanKind::Italic};});
    registerCommand("rmfamily",[](Engine& e){e.styles_.clear();});
    installExpansion(); installRegisters();
}
}
