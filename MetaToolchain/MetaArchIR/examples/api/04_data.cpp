#include "../../MetaArchIR.hpp"
#include <iostream>
int main(){mair::Program p;p.section(".data").global("value").label("value").data("quad",{"42"});std::cout<<mair::render(p,mair::target("amd64"));}
