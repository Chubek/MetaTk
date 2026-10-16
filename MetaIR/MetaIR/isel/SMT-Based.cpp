#include "metachain/ISel.hpp"
#include "satie/BackendSAT.hpp"

namespace metachain::isel {
Result smt_cover(const Graph& g,const std::vector<Rule>& rules,Options o) {
    Result r; auto tiles=candidates(g,rules,o,r); if (!r) return r;
    // Finite Boolean exact-cover encoding, with a unary weighted cost bound.
    // This is the SAT fragment of SMT; no external SMT process is required.
    try {
        sat::Formula f; std::vector<int> variable,weighted;
        for (const auto& t:tiles) {
            variable.push_back(f.fresh()); auto cost=rules[t.rule].cost;
            if (cost>4096||weighted.size()+cost>4096) { r.diagnostics.push_back({"SAT selection cost encoding limit exceeded",t.root}); return r; }
            weighted.insert(weighted.end(),cost,variable.back());
        }
        for (Id n=0;n<g.nodes.size();++n) {
            std::vector<int> covers;
            for (Id t=0;t<tiles.size();++t) if (std::find(tiles[t].covered.begin(),tiles[t].covered.end(),n)!=tiles[t].covered.end()) covers.push_back(variable[t]);
            f.exactly_one(covers);
        }
        auto model=sat::solve(f);
        if (!model.satisfiable) { r.diagnostics.push_back({"no complete SAT instruction cover",0}); return r; }
        auto cost=[&](const sat::Model& m){std::size_t c=0; for (auto x:weighted) if (m.values[x]) ++c; return c;};
        std::size_t low=0,high=cost(model),steps=0;
        while (low<high) {
            if (++steps>o.max_search) { r.diagnostics.push_back({"SAT optimization search limit exceeded",0}); return r; }
            auto mid=low+(high-low)/2; auto bounded=f; bounded.at_most(weighted,mid);
            auto candidate=sat::solve(bounded);
            if (candidate.satisfiable) { high=cost(candidate); model=std::move(candidate); } else low=mid+1;
        }
        for (Id t=0;t<tiles.size();++t) if (model.values[variable[t]]) r.tiles.push_back(tiles[t]);
        r.cost=high; r.optimal=true; order_tiles(r,g);
    } catch (const std::length_error& e) { r.diagnostics.push_back({e.what(),0}); }
    return r;
}
} // namespace metachain::isel
