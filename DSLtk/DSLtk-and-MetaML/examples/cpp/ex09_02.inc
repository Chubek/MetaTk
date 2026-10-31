dsl::EGraph g;
auto a = g.add("(add x 0)");
auto b = g.add("x");
g.unite(a, b);
assert(g.equivalent(a, b));
