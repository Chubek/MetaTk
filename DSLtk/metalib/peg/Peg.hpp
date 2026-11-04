#pragma once
#include "../cpp/Parsec.h"
namespace metalib {
inline void install_Peg(metaml::Session &s) { if (!s.find_primitive("Parsec.char")) install_Parsec(s); }
}
