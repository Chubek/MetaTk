#include "../../MetaArchIR.hpp"
#include <iostream>
int main(){mair::Program p;p.section(".text").global("add2").label("add2").ins("mov","%edi","%eax").ins("add","%esi","%eax").ins("ret");std::cout<<mair::render(p,mair::target("amd64"));}
