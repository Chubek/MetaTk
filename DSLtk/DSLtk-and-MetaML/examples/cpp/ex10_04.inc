dsl::CFG g;
g.start = "S";
g.add("S", {dsl::CFGSymbol::token("name")});
assert(g.parse({"name"}).ok());
assert(!g.parse({"other"}).ok());
