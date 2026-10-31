#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
dsl::CFG g;
g.start = "S";
g.add("S", {});
assert(g.parse({}).ok());
assert(!g.parse({"x"}).ok());
}
