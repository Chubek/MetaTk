#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
dsl::EGraph g;
auto a = g.add("(add x 0)");
auto b = g.add("x");
g.unite(a, b);
assert(g.equivalent(a, b));
}
