#include "metachain/ISel.hpp"

namespace metachain::isel {
// Graph covering over selection DAGs. Cyclic graphs cannot be tiled directly:
// report a concrete cycle witness so callers can split the graph into
// acyclic regions (e.g., one per loop body or trace) before selecting.
Result graph_cover(const Graph& g,const std::vector<Rule>& rules,Options o) {
    if (!backend::verify(g).empty()) {
        Result r; r.diagnostics=backend::verify(g); return r;
    }
    if (auto cycle=backend::find_cycle(g)) {
        Result r;
        std::string message="selection graph contains a cycle:";
        for (std::size_t k=0;k<cycle->size()&&k<8;++k) message+=" %"+std::to_string((*cycle)[k]);
        r.diagnostics.push_back({std::move(message),(*cycle)[0]});
        return r;
    }
    return cover(g,rules,o);
}
} // namespace metachain::isel
