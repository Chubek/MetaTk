#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
s.register_primitive("Host.twice", [](std::int64_t n) { return n * 2; });
auto r = s.eval("Host.twice 21");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
