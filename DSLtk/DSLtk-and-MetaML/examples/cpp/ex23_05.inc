auto before = dsl::parse_sexpr("(add 42 0)");
dsl::TermRewriteSystem rs;
rs.add_rule("(rewrite az (add ?x 0) ?x)");
auto r = rs.normalize(dsl::term_from_sexpr(before));
assert(r.term.dump() == "42");
assert(before.dump() == "(add 42 0)");
