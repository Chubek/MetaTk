auto rs = new TermRewriteSystem();
rs.add_rule("(rewrite az (add ?x 0) ?x)");
auto r = rs.normalize(parse_term("(add 42 0)"), 0);
assert(!r.normal_form());
assert(r.steps == 0);
assert(r.term.dump() == "(add 42 0)");
