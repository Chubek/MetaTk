#include "metachain/Regalloc.hpp"
#include <algorithm>
#include <numeric>
namespace metachain::regalloc {
Result briggs(const InterferenceGraph& g,unsigned colors) {
    Result r; r.diagnostics=verify(g,colors); if (!r) return r;
    std::vector<backend::Id> parent(g.neighbors.size()); std::iota(parent.begin(),parent.end(),0);
    auto root=[&](backend::Id n){while (parent[n]!=n) n=parent[n]; return n;};
    auto collapsed=[&] {
        InterferenceGraph c; c.neighbors.resize(parent.size()); c.spill_cost.resize(parent.size());
        for (backend::Id n=0;n<parent.size();++n) {
            auto a=root(n); c.spill_cost[a]+=g.spill_cost.empty()?1:g.spill_cost[n];
            for (auto m:g.neighbors[n]) c.add_edge(a,root(m));
        }
        for (auto [n,color]:g.precolored) c.precolored[root(n)]=color;
        return c;
    };
    for (auto [x,y]:g.moves) {
        auto a=root(x),b=root(y); if (a==b) continue;
        auto c=collapsed(); if (c.neighbors[a].count(b)) continue;
        // Keep fixed registers distinct; Briggs' conservative test applies to
        // pairs of non-precolored nodes (George coalescing is a separate policy).
        if (c.precolored.count(a)||c.precolored.count(b)) continue;
        auto neighbors=c.neighbors[a]; neighbors.insert(c.neighbors[b].begin(),c.neighbors[b].end());
        std::size_t high=0; for (auto n:neighbors) if (c.neighbors[n].size()>=colors) ++high;
        if (high<colors) parent[b]=a;
    }
    auto c=collapsed(); auto allocated=color(c,colors); if (!allocated) return allocated;
    r.registers.resize(parent.size()); std::map<backend::Id,std::size_t> slots;
    for (backend::Id n=0;n<parent.size();++n) {
        auto a=root(n); r.registers[n]=allocated.registers[a];
        if (!r.registers[n]) { if (!slots.count(a)) slots[a]=slots.size(); r.spills[n]=slots[a]; }
    }
    return r;
}
} // namespace metachain::regalloc
