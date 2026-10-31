auto rs = new TermRewriteSystem();
rs.add_rule("(rewrite z (add ?x 0) ?x)");
auto r = rs.normalize(parse_term("(add a 0)"), 20);
assert(r.normal_form());
assert(r.term.dump() == "a");
