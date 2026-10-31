auto g = new EGraph();
g.add("x");
SaturationLimits limits;
limits.iterations = 0;
auto r = g.saturate([], limits);
assert(r.stop == SaturationStop.IterationLimit);
