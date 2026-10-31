#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("val make := fn n -> fn x -> n * x\nval six := make 6\nsix 7");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
