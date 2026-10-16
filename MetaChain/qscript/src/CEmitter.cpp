#include "qscript/QScript.h"
#include "metachain/CSource.hpp"
namespace qscript {
Result<std::string> emit_c(const metail::Program& p,Options o) {
    auto verified=verify_ir(p,o);
    if (!verified) return {std::nullopt,std::move(verified.diagnostics)};
    auto source=metachain::lower_c99(p,o.toolchain.vm,o.toolchain.assembler.max_source_bytes);
    Result<std::string> result;
    for (const auto& d:source.diagnostics)
        result.diagnostics.push_back({metachain::Stage::Lower,d.message,d.source.file,d.source.line,d.source.column,d.instruction});
    if (source) result.value=std::move(source.source);
    return result;
}
}
