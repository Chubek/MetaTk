#include "metachain/ISel.hpp"
namespace metachain::isel {
Result graph_cover(const Graph& g,const std::vector<Rule>& rules,Options o) {
    // Cyclic control graphs must be split into acyclic selection regions first.
    return cover(g,rules,o);
}
} // namespace metachain::isel
