#include "qscript/QScript.h"
namespace qscript {
Result<metail::Program> optimize(metail::Program p,Options o) {
    auto checked=verify_ir(std::move(p),o); if (!checked) return checked;
    auto optimized=metail::optimize(std::move(*checked));
    return verify_ir(std::move(optimized.program),o);
}
} // namespace qscript
