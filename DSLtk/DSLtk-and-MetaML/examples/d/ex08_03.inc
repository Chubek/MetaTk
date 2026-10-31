auto p = parse_term("(pair ?x ?x)");
auto good = parse_term("(pair a a)");
auto bad = parse_term("(pair a b)");
assert(!match_term(p, good).isNull);
assert(match_term(p, bad).isNull);
