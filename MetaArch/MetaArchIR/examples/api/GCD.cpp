#include "../../MetaArchIR.hpp"
#include <iostream>

// SysV amd64: unsigned gcd(unsigned a, unsigned b), Euclid by subtraction.
// Physical registers and branch instructions are explicit portable-assembly data.
int main() {
    mair::Program p;
    p.section(".text").global("gcd").label("gcd")
     .ins("mov", "%edi", "%eax").ins("test", "%esi", "%esi").ins("jz", ".done")
     .ins("test", "%eax", "%eax").ins("jz", ".take_b")
     .label(".loop").ins("cmp", "%esi", "%eax").ins("je", ".done").ins("jb", ".reduce_b")
     .ins("sub", "%esi", "%eax").ins("jmp", ".loop")
     .label(".reduce_b").ins("sub", "%eax", "%esi").ins("jmp", ".loop")
     .label(".take_b").ins("mov", "%esi", "%eax")
     .label(".done").ins("ret");
    std::cout << mair::render(p, mair::target("amd64"));
}
