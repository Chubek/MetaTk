auto g = new DirectedGraph!();
auto a = g.add_node("input");
auto b = g.add_node("output");
g.add_edge(a, b);
auto order = g.topological_sort();
assert(order[0] == a && order[$-1] == b);
