#include "GraphIR.hpp"

// Value graph: SSA value dependencies without control. Effect nodes keep
// their relative order edges from GSSA; control and state tokens live in
// VSDG/sea-of-nodes instead.
namespace metachain::graph {
// Value graph: SSA value dependencies without control. Effect nodes keep
// their relative order edges from GSSA; control and state tokens live in
// VSDG/sea-of-nodes instead.
Analysis value_graph(const metail::Program& p) {
    auto s=gssa(p);
    return {std::move(s.graph),std::move(s.diagnostics)};
}
} // namespace metachain::graph
