#include "metachain/ISel.hpp"
#include <algorithm>

namespace metachain::isel {
Result tree_cover(const Graph& g,const std::vector<Rule>& rules,Options o) {
    Result r; auto tiles=candidates(g,rules,o,r); if (!r) return r;
    std::vector<unsigned> users(g.nodes.size());
    for (const auto& n:g.nodes) for (auto i:n.inputs) if (++users[i]>1) {
        r.diagnostics.push_back({"tree covering does not accept shared operands",i}); return r;
    }
    const auto inf=std::numeric_limits<std::uint64_t>::max();
    std::vector<std::uint64_t> cost(g.nodes.size(),inf);
    std::vector<std::optional<Id>> choice(g.nodes.size());
    std::vector<std::vector<Id>> at(g.nodes.size());
    for (Id t=0;t<tiles.size();++t) at[tiles[t].root].push_back(t);
    auto topo=*backend::topological_order(g);
    std::size_t steps=0;
    for (auto n:topo) for (auto t:at[n]) {
        if (++steps>o.max_search) { r.diagnostics.push_back({"tree selection work limit exceeded",n}); return r; }
        std::uint64_t c=rules[tiles[t].rule].cost;
        for (auto operand:tiles[t].operands) {
            if (cost[operand]==inf||c>inf-cost[operand]) { c=inf; break; }
            c+=cost[operand];
        }
        if (c<cost[n]) { cost[n]=c; choice[n]=t; }
    }
    std::vector<Id> work;
    for (Id n=0;n<g.nodes.size();++n) if (!users[n]) work.push_back(n);
    while (!work.empty()) {
        auto n=work.back(); work.pop_back();
        if (!choice[n]) { r.diagnostics.push_back({"no tree instruction cover",n}); return r; }
        const auto& t=tiles[*choice[n]];
        if (r.cost>inf-rules[t.rule].cost) { r.diagnostics.push_back({"tree selection cost overflow",n}); return r; }
        r.tiles.push_back(t); r.cost+=rules[t.rule].cost;
        work.insert(work.end(),t.operands.begin(),t.operands.end());
    }
    r.optimal=true; order_tiles(r,g); return r;
}
} // namespace metachain::isel
