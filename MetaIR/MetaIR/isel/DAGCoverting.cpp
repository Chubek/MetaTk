#include "metachain/ISel.hpp"

namespace metachain::isel {
// DAG covering via bounded exact search. Shared value nodes are covered once
// and charged once; every tile's internal nodes must be used only inside the
// tile so no observation or effect is duplicated or lost.
Result dag_cover(const Graph& g,const std::vector<Rule>& rules,Options o) {
    Result r; auto tiles=candidates(g,rules,o,r); if (!r) return r;
    std::size_t shared=0, max_users=0;
    {
        std::vector<std::size_t> uses(g.nodes.size());
        for (const auto& n:g.nodes) for (auto i:n.inputs) ++uses[i];
        for (auto u:uses) {
            shared+=u>1?1:0;
            max_users=std::max(max_users,u);
        }
    }
    // Small DAGs without sharing reduce to tree covering; keep the general
    // exact search for the shared case so costs stay comparable with SMT.
    if (shared==0) {
        auto trees=tree_cover(g,rules,o);
        if (trees) return trees;
        // Fall through to the exact search if tree labeling failed.
        r.diagnostics.clear();
    }
    (void)max_users;
    return cover(g,rules,o);
}
} // namespace metachain::isel
