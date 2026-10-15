#include "qscript/QScript.h"
namespace qscript {
Result<metail::Program> compile(std::string_view source,Options o,std::string file) {
    auto ast=parse(source,o,file); if (!ast) return {std::nullopt,std::move(ast.diagnostics)};
    auto p=ast_to_ir(*ast,o); if (!p) return p;
    p->name=std::move(file);
    if (o.toolchain.optimize) { p=optimize(std::move(*p),o); if (!p) return p; }
    return schedule(std::move(*p),o);
}
Result<std::vector<std::uint8_t>> build(const metail::Program& p,Options o) {
    auto assembly=select(p,o); if (!assembly) return {std::nullopt,std::move(assembly.diagnostics)};
    auto object=assemble(*assembly,true,o); if (!object) return object;
    return link({*object},o);
}
} // namespace qscript
