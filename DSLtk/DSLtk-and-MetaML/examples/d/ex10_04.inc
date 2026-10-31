CFG g;
g.start = "S";
g.add("S", [CFGSymbol.token("name")]);
assert(g.parse(["name"]).ok());
assert(!g.parse(["other"]).ok());
