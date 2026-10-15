#include "qscript/QScript.h"
namespace qscript {
Result<metail::Program> verify_ir(metail::Program p,Options o) {
    Result<metail::Program> r;
    for (const auto& e:metail::verify(p,o.toolchain.il)) r.diagnostics.push_back({metachain::Stage::Verify,e.message,e.source.file,e.source.line,e.source.column,e.instruction});
    if (r.diagnostics.empty()) r.value=std::move(p);
    return r;
}
} // namespace qscript
