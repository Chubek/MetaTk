#include "../../MetaArchIR.hpp"
#include <iostream>
int main(){mair::Program p;p.section(".text").global("max2").label("max2").ins("cmp","%esi","%edi").ins("jge","done").label("done").ins("ret");std::cout<<mair::render(p,mair::target("amd64"));}
