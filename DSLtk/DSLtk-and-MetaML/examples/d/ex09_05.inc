auto g = new EGraph();
auto large = g.add("(id x)");
auto small = g.add("x");
g.unite(large, small);
assert(g.extract(large).dump() == "x");
