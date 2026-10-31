#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
assert(s.eval("val base := 40").is_ok());
auto r = s.eval("base + 2");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
