#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("if 6 < 7 then 42 else 0");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
