#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto n = dsl::node<"sub">(
  dsl::leaf<"integer">(9), dsl::leaf<"integer">(4));
assert(!n.is_leaf());
assert(n.size() == 2);
assert(n.child(0).value() == "9");
}
