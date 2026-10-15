#include "GraphIR.hpp"

namespace metachain::graph {
Analysis dag(const Graph& g) {
    Analysis r{g,backend::verify(g)};
    if (r&&!backend::topological_order(g)) r.diagnostics.push_back({"graph contains a cycle",0});
    return r;
}
} // namespace metachain::graph
