#pragma once
#include "../cpp/Buffer.h"
namespace metalib {
inline void install_Memory(metaml::Session &s) {
  if (!s.find_primitive("Buffer.create")) install_Buffer(s);
}
}
