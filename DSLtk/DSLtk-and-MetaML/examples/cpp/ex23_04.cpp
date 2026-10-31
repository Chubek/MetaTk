#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
dsl::TermRewriteSystem rs;
rs.add_rule("(rewrite az (add ?x 0) ?x)");
auto r = rs.normalize(dsl::parse_term("(add 42 0)"), 0);
assert(!r.normal_form());
assert(r.steps == 0);
assert(r.term.dump() == "(add 42 0)");
}
