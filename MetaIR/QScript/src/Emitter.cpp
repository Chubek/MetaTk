#include "qscript/QScript.h"
namespace qscript {
Result<std::string> emit(const metail::Program& p,Options o) {
    auto checked=verify_ir(p,o); if (!checked) return {std::nullopt,std::move(checked.diagnostics)};
    return {metail::emit(p),{}};
}
} // namespace qscript
