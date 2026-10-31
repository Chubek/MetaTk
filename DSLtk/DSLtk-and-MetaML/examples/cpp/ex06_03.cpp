#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto g = dsl::create_peg_definition();
g.add_rule<"ab">();
g.add_rule<"a">();
assert(g.parse("ab").ok());
}
