auto p = dsl::parse_term("(pair ?x ?x)");
auto good = dsl::parse_term("(pair a a)");
auto bad = dsl::parse_term("(pair a b)");
assert(dsl::match_term(p, good).has_value());
assert(!dsl::match_term(p, bad).has_value());
