#include "MetaChain.hpp"
#include <iostream>

int main() {
    using namespace metachain::native;
    Script script;
    auto answer = reg(0), factor = reg(1), one = reg(2);
    script.set(answer, 1).set(factor, 6).set(one, 1)
        .label("loop")
        .set(answer, answer * factor)
        .set(factor, factor - one)
        .while_nonzero(factor, "loop")
        .print(answer).halt(answer);
    auto program = script.finish();
    if (!program) return 1;
    metachain::Toolchain tc;
    auto execution = tc.run(*program);
    if (!execution) {
        for (const auto& d : execution.diagnostics) std::cerr << metachain::format(d) << '\n';
        return 1;
    }
    for (auto v : execution->output) std::cout << v << '\n';
    return execution->value == 720 ? 0 : 1;
}
