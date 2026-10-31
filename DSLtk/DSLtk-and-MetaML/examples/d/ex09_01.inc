auto g = new EGraph();
auto a = g.add("(add x 0)");
auto b = g.add("(add x 0)");
assert(g.equivalent(a, b));
