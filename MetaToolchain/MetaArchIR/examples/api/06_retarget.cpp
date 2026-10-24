#include "../../MetaArchIR.hpp"
#include <iostream>

// The same explicit assembly operation is encoded by three database backends.
int main() {
    mair::Program p;
    p.section(".text").global("return_to_caller").label("return_to_caller").encode("ret");
    for (const auto *arch : {"amd64", "aarch64", "riscv64"})
        std::cout << mair::render(p, mair::target(arch));
}
