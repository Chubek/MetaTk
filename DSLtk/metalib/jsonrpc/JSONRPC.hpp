#pragma once
#include "../cpp/JSON.h"
namespace metalib {
inline void install_JSONRPC(metaml::Session &s) {
  if (!s.find_primitive("JSON.parse")) install_JSON(s);
}
}
