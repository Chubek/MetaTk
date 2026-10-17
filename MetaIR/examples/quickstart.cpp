// Builds 6 * 7 with the native C++ DSL, runs it, and exits with 0 when it yields 42.
#include "MetaChain.hpp"

int main() {
    using namespace metachain::native;
    Script code;
    auto result = reg(0);
    code.set(result, Expr(6) * Expr(7)).halt(result);
    auto program = code.finish();
    if (!program) return 1;
    auto executed = metachain::Toolchain().run(*program);
    return executed && executed->value == 42 ? 0 : 1;
}
