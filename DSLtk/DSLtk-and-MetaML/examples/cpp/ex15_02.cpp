#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("val add := fn x y -> x + y\nval add40 := add 40\nadd40 2");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
