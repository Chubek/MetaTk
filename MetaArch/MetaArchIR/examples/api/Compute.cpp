#include "../../MetaArchIR.hpp"
#include <iostream>

// SysV amd64: uint32_t compute(uint32_t x, uint32_t y) = x*x + y*y.
int main() {
    mair::Program p;
    p.section(".text").global("compute").label("compute")
     .ins("mov", "%edi", "%eax").ins("imul", "%edi", "%eax")
     .ins("imul", "%esi", "%esi").ins("add", "%esi", "%eax").ins("ret");
    std::cout << mair::render(p, mair::target("amd64"));
}
