auto s = parse_sexpr("(add 1 2)");
assert(!s.is_atom);
assert(s.items.length == 3);
assert(s.dump() == "(add 1 2)");
