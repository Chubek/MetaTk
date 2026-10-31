auto g = create_peg_definition();
g.add_rule!"ab"();
g.add_rule!"a"();
assert(g.parse("ab").ok());
