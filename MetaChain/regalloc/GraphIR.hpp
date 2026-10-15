#pragma once
#include "metachain/Backend.hpp"
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <vector>

namespace metachain::regalloc {
struct InterferenceGraph {
    std::vector<std::set<backend::Id>> neighbors;
    std::map<backend::Id,unsigned> precolored;
    std::vector<double> spill_cost;
    std::vector<std::pair<backend::Id,backend::Id>> moves;
    void add_edge(backend::Id a,backend::Id b) {
        if (a>=neighbors.size()||b>=neighbors.size()) throw std::out_of_range("interference vertex");
        if (a!=b) { neighbors[a].insert(b); neighbors[b].insert(a); }
    }
};
inline std::vector<backend::Diagnostic> verify(const InterferenceGraph& g,unsigned colors) {
    std::vector<backend::Diagnostic> e;
    if (!colors) e.push_back({"register count must be positive",0});
    if (!g.spill_cost.empty()&&g.spill_cost.size()!=g.neighbors.size()) e.push_back({"spill cost size mismatch",0});
    for (backend::Id n=0;n<g.neighbors.size();++n) for (auto m:g.neighbors[n])
        if (m>=g.neighbors.size()||m==n||!g.neighbors[m].count(n)) e.push_back({"invalid asymmetric interference edge",n});
    for (auto [n,c]:g.precolored) {
        if (n>=g.neighbors.size()||c>=colors) { e.push_back({"invalid precolored register",n}); continue; }
        for (auto m:g.neighbors[n]) {
            auto p=g.precolored.find(m);
            if (p!=g.precolored.end()&&p->second==c) e.push_back({"interfering precolored registers",n});
        }
    }
    for (auto [a,b]:g.moves) if (a>=g.neighbors.size()||b>=g.neighbors.size()) e.push_back({"invalid move vertex",a});
    for (double cost:g.spill_cost) if (!(cost>=0) || cost>std::numeric_limits<double>::max()) e.push_back({"invalid spill cost",0});
    return e;
}
} // namespace metachain::regalloc
