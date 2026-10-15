#include "GraphIR.hpp"

namespace metachain::graph {
Analysis value_graph(const metail::Program& p) {
    auto s=gssa(p);
    return {std::move(s.graph),std::move(s.diagnostics)};
}
} // namespace metachain::graph
