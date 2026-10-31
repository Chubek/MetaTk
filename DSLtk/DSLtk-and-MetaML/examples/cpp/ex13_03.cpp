#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("val x := 6\nval y := 7\nx * y");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
