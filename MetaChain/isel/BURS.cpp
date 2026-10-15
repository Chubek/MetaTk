#include "metachain/ISel.hpp"
namespace metachain::isel {
Result burs(const Graph& g,const std::vector<Rule>& rules,Options o) {
    // Bottom-up cost labeling and top-down reduction for a single value class.
    // Rule patterns are structural, so no separate nonterminal automaton is needed.
    return tree_cover(g,rules,o);
}
} // namespace metachain::isel
