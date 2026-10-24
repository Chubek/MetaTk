#include "../../MetaArchIR.hpp"
#include <iostream>

int main() {
    mair::Program p;
    p.encode("call", {{"sym", mair::symbol("external_function", -4)}});
    auto image = mair::assemble(p, mair::target("amd64"));
    for (const auto &r : image.relocations)
        std::cout << r.name << " at " << r.offset << " -> " << r.symbol << " addend " << r.value << '\n';
}
