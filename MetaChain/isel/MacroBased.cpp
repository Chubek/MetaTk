#include "metachain/ISel.hpp"
namespace metachain::isel {
Result macro_cover(const Graph& g,const std::vector<Rule>& rules,Options o) {
    // Prefer inexpensive large macros; backtrack if a macro prevents completion.
    return cover(g,rules,o,true);
}
} // namespace metachain::isel
