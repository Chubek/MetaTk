#pragma once
#include "MetaTeX.h"
namespace vmdesc {
/* Install into the caller's MetaTeX engine; handlers have no captured state.
 * MetaTeX owns expansion, formatting, diagnostics and output devices. */
inline void install(metatex::Engine& engine) {
    engine.registerCommand("VMName",[](metatex::Engine&e){ e.setTitle(e.readTextArgument()); });
    engine.registerCommand("VMArchitecture",[](metatex::Engine&e){
        auto text=e.readTextArgument(); e.paragraph();e.addText("Architecture: "+text);e.paragraph();
    });
    engine.registerCommand("VMStatus",[](metatex::Engine&e){
        auto text=e.readTextArgument();e.paragraph();e.addText("Implementation status: "+text);e.paragraph();
    });
    engine.registerCommand("VMInstruction",[](metatex::Engine&e){
        auto name=e.readTextArgument(),operands=e.readTextArgument(),semantics=e.readTextArgument();
        e.paragraph();e.addSpan(metatex::SpanKind::Code,name+" "+operands);e.addText(" — "+semantics);e.paragraph();
    });
    engine.registerCommand("VMInvariant",[](metatex::Engine&e){
        auto text=e.readTextArgument();e.paragraph();e.addText("Invariant: "+text);e.paragraph();
    });
}
}
