#include "../../MetaArchIR.hpp"
#include <iostream>
int main(){mair::Program p;p.section(".text").global("answer").label("answer").ins("mov","$42","%eax").ins("ret");std::cout<<mair::render(p,mair::target("amd64"));}
