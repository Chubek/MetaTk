#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("val r := {width = 6, height = 7}\nr.width * r.height");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
