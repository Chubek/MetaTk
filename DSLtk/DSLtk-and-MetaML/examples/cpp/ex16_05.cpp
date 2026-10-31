#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("val r := {width = 1, height = 7}\nval q := {r with width = 6}\nq.width * q.height");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
