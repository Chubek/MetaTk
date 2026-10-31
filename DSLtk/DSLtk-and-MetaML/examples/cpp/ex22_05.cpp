#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("val square := fn x -> x * x\nsquare 6 + 6");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
