#include "GraphIR.hpp"

namespace metachain::graph {
Analysis pdg(const metail::Program& p) {
    auto r=dfg(p); if (!r) return r;
    auto c=cfg(p); const Id count=c.blocks.size(),exit=count;
    std::set<Id> all; for (Id b=0;b<=count;++b) all.insert(b);
    std::vector<std::set<Id>> post(count+1,all); post[exit]={exit};
    // Reject non-exiting regions rather than inventing postdominators.
    std::set<Id> exits; std::vector<Id> work;
    for (Id b=0;b<count;++b) if (c.blocks[b].successors.empty()) work.push_back(b);
    while (!work.empty()) { auto b=work.back(); work.pop_back(); if (!exits.insert(b).second) continue; for (auto pred:c.blocks[b].predecessors) work.push_back(pred); }
    if (exits.size()!=count) { r.diagnostics.push_back({"control dependence requires a path to exit from every block",0}); return r; }
    bool changed=true;
    while (changed) {
        changed=false;
        for (Id b=0;b<count;++b) {
            auto successors=c.blocks[b].successors; if (successors.empty()) successors.push_back(exit);
            auto d=all;
            for (auto s:successors) { std::set<Id> common; std::set_intersection(d.begin(),d.end(),post[s].begin(),post[s].end(),std::inserter(common,common.end())); d=std::move(common); }
            d.insert(b); if (d!=post[b]) { post[b]=std::move(d); changed=true; }
        }
    }
    for (Id b=0;b<count;++b) if (c.blocks[b].successors.size()>1) {
        for (auto s:c.blocks[b].successors) for (auto controlled:post[s])
            if (controlled<count&&(!post[b].count(controlled)||controlled==b))
                for (Id pc=c.blocks[controlled].begin;pc<c.blocks[controlled].end;++pc) {
                    auto& inputs=r.graph.nodes[pc].inputs; auto branch=c.blocks[b].end-1;
                    if (pc!=branch&&std::find(inputs.begin(),inputs.end(),branch)==inputs.end()) inputs.push_back(branch);
                }
    }
    return r;
}
} // namespace metachain::graph
