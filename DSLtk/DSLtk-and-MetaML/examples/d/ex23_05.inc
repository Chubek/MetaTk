auto before = parse_sexpr("(add 42 0)");
auto rs = new TermRewriteSystem();
rs.add_rule("(rewrite az (add ?x 0) ?x)");
auto r = rs.normalize(term_from_sexpr(before));
assert(r.term.dump() == "42");
assert(before.dump() == "(add 42 0)");
