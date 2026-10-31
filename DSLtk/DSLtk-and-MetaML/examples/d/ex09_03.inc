auto g = new EGraph();
auto root = g.add("(add x 0)");
auto rules = sexpr_rules("(rewrite z (add ?x 0) ?x)");
auto report = g.saturate(rules);
assert(report.saturated());
assert(g.extract(root).dump() == "x");
