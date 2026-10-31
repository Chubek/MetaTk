#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto n = dsl::node<"add">(dsl::leaf<"n">(1),
                           dsl::leaf<"n">(2));
assert(n.dump() == "(add (n 1) (n 2))");
}
