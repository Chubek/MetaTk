#include "MetaTeX.h"
#include <string>
using namespace metatex;
extern "C" const char* metatex_plugin_id(){return "nekotex";}
extern "C" void metatex_register_plugin(Engine* engine){
    Engine& e=*engine;
    e.registerCommand("nekotex",[](Engine& x){x.addText("NekoTeX");});
    e.registerCommand("note",[](Engine& x){x.startBlock(BlockKind::Note);x.addText("Note: ");x.execute(x.readArgument());x.paragraph();});
    e.registerCommand("tip",[](Engine& x){x.startBlock(BlockKind::Note);x.addText("Tip: ");x.execute(x.readArgument());x.paragraph();});
    e.registerCommand("warning",[](Engine& x){x.startBlock(BlockKind::Note);x.addText("Warning: ");x.execute(x.readArgument());x.paragraph();});
    e.registerCommand("callout",[](Engine& x){auto variant=x.readOptionalText("Information");x.startBlock(BlockKind::Note);x.addText(variant+": ");x.execute(x.readArgument());x.paragraph();});
    e.registerCommand("codeblock",[](Engine& x){auto language=x.readOptionalText();(void)language;
        x.startBlock(BlockKind::Code);x.addText(x.readTextArgument());x.paragraph();});
    e.registerCommand("component",[](Engine& x){auto name=x.readTextArgument();x.startBlock(BlockKind::Note);x.addText("["+name+"] ");x.execute(x.readArgument());x.paragraph();});
    e.registerCommand("exercise",[](Engine& x){x.startBlock(BlockKind::Note);x.addText("Exercise: ");x.execute(x.readArgument());x.paragraph();});
    e.registerCommand("concept",[](Engine& x){x.addSpan(SpanKind::Bold,x.readTextArgument());});
    e.registerCommand("kbd",[](Engine& x){x.addSpan(SpanKind::Code,x.readTextArgument());});
    e.registerCommand("inlinecode",[](Engine& x){x.addSpan(SpanKind::Code,x.readTextArgument());});
    e.registerCommand("meta",[](Engine& x){x.addSpan(SpanKind::SmallCaps,x.readTextArgument());});
    e.registerCommand("separator",[](Engine& x){x.startBlock(BlockKind::HorizontalRule);x.paragraph();});
}
