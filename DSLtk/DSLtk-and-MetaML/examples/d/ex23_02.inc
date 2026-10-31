auto rs = new TermRewriteSystem();
rs.add_rule("(rewrite az (add ?x 0) ?x)");
rs.add_rule("(rewrite mo (mul ?x 1) ?x)");
auto r = rs.normalize(parse_term("(mul (add 42 0) 1)"), 100);
assert(r.normal_form());
assert(r.term.dump() == "42");
