#include "../../MetaArchIR.hpp"
#include <iostream>
int main(){mair::Program p;p=mair::parse("section .text\nglobal f\nf:\n ret\n");std::cout<<mair::render(p,mair::target("amd64"));}
