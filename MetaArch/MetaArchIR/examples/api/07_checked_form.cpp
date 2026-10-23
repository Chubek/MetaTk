#include "../../MetaArchIR.hpp"
#include <iostream>

int main() {
    auto target = mair::target("riscv64");
    // Resolve the form by metadata rather than pinning its current numeric index.
    for (size_t i : target.forms("addi")) {
        mair::Program p;
        p.form(i, {{"rd", mair::reg("x10")}, {"rs1", mair::reg("x10")}, {"imm", mair::imm(-1)}});
        std::cout << mair::render(p, target); // 13 05 f5 ff
        return 0;
    }
    return 1;
}
