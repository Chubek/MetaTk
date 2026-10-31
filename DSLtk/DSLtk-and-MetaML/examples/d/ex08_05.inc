auto g = new TermGraph();
auto root = g.add("(pair (f a) (f a))");
assert(g.to_term(root).dump() == "(pair (f a) (f a))");
assert(g.graph().node_count() == 3);
