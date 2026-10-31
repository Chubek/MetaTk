auto g = create_peg_definition();
g.add_rule!"[0-9]+"();
assert(g.parse("123").ok());
assert(!g.parse("12x").ok());
