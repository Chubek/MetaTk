auto x = dsl::parse_sexpr("(add 42 0)");
assert(!x.is_atom && x.items.size() == 3);
assert(x.items[0].text == "add");
assert(x.items[1].text == "42");
