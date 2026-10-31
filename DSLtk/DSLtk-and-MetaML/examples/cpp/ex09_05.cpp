#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
dsl::EGraph g;
auto large = g.add("(id x)");
auto small = g.add("x");
g.unite(large, small);
assert(g.extract(large).dump() == "x");
}
