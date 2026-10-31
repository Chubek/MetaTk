#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
dsl::EGraph g;
auto root = g.add("(mul (add 42 0) 1)");
auto rules = dsl::sexpr_rules(
 "(rewrite az (add ?x 0) ?x) (rewrite mo (mul ?x 1) ?x)");
auto r = g.saturate(rules);
assert(r.saturated());
assert(g.extract(root).dump() == "42");
}
