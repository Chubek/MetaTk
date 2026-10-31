#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
dsl::EGraph g;
g.add("x");
dsl::SaturationLimits limits;
limits.iterations = 0;
auto r = g.saturate({}, limits);
assert(r.stop == dsl::SaturationStop::IterationLimit);
}
