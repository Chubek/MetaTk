#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto r = metaml::parse("val x := 42", "unit.ml");
assert(r.ok());
assert(r.program.items.size() == 1);
}
