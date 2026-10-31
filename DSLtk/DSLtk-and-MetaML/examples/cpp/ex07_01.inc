auto s = dsl::parse_sexpr("(add 1 2)");
assert(!s.is_atom);
assert(s.items.size() == 3);
assert(s.dump() == "(add 1 2)");
