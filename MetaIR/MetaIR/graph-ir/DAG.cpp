#include "GraphIR.hpp"

namespace metachain::graph {
// Validate that a dependence/value graph is acyclic and ready for selection
// or scheduling. Cyclic graphs must be split into regions first; the
// diagnostic names a concrete cycle witness when one exists.
Analysis dag(const Graph& g) {
    Analysis r{g,backend::verify(g)};
    if (!r) return r;
    if (!backend::topological_order(g)) {
        if (auto cycle=backend::find_cycle(g)) {
            std::string message="graph contains a cycle:";
            for (std::size_t k=0;k<cycle->size()&&k<8;++k) message+=" %"+std::to_string((*cycle)[k]);
            r.diagnostics.push_back({std::move(message),(*cycle)[0]});
        } else {
            r.diagnostics.push_back({"graph contains a cycle",0});
        }
    }
    return r;
}
} // namespace metachain::graph
