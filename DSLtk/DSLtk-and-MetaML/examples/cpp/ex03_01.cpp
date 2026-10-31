#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto n = dsl::leaf<"integer">(42);
assert(n.is_leaf());
assert(n.tag() == "integer");
assert(n.value() == "42");
}
