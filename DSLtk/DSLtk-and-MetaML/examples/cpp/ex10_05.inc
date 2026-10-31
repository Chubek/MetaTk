dsl::CFG g;
g.start = "S";
g.add("S", {});
assert(g.parse({}).ok());
assert(!g.parse({"x"}).ok());
