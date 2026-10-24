#include "../../MetaArchIR.hpp"
#include <iostream>

int main() {
    mair::Program p;
    p.section(".text").global("entry").label("entry").encode("ret")
     .section(".rodata").align(8).label("message").ascii("Hello\n")
     .data("byte", {"0"});
    auto image = mair::assemble(p, mair::target("aarch64"));
    for (const auto &section : image.sections)
        std::cout << section.name << ": " << section.bytes.size() << " bytes\n";
}
