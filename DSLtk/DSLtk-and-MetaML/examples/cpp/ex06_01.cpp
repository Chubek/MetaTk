#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto g = dsl::create_peg_definition();
g.add_rule<"[0-9]+">();
assert(g.parse("123").ok());
assert(!g.parse("12x").ok());
}
