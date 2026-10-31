#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto names = s.primitive_names();
assert(!names.empty());
assert(s.find_primitive("Int.parse") != nullptr);
}
