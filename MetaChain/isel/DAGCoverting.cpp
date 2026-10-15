#include "metachain/ISel.hpp"
namespace metachain::isel {
Result dag_cover(const Graph& g,const std::vector<Rule>& rules,Options o) {
    // Unlike tree covering, exact cover charges a shared node only once.
    return cover(g,rules,o);
}
} // namespace metachain::isel
