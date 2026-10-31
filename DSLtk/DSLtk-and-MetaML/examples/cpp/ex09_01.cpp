#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
dsl::EGraph g;
auto a = g.add("(add x 0)");
auto b = g.add("(add x 0)");
assert(g.equivalent(a, b));
}
