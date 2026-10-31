auto g = new DirectedGraph!();
auto a = g.add_node("parse");
auto b = g.add_node("check");
g.add_edge(a, b, "next");
assert(g.successors(a)[0] == b);
