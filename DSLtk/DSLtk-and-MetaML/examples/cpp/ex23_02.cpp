#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
dsl::TermRewriteSystem rs;
rs.add_rule("(rewrite az (add ?x 0) ?x)");
rs.add_rule("(rewrite mo (mul ?x 1) ?x)");
auto r = rs.normalize(dsl::parse_term("(mul (add 42 0) 1)"), 100);
assert(r.normal_form());
assert(r.term.dump() == "42");
}
