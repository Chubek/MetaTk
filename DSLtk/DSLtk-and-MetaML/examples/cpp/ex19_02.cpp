#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
s.register_primitive("Host.add",
  [](std::int64_t a, std::int64_t b) { return a + b; });
auto r = s.eval("Host.add 19 23");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
