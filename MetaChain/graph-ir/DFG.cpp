#include "GraphIR.hpp"

namespace metachain::graph {
Analysis dfg(const metail::Program& p) {
    Analysis r; auto c=cfg(p); r.diagnostics=c.diagnostics; if (!r) return r;
    r.graph.nodes.resize(p.size());
    using Definitions=std::array<std::set<Id>,32>;
    std::vector<Definitions> entry(c.blocks.size()),exit(c.blocks.size());
    bool changed=true;
    while (changed) {
        changed=false;
        for (Id b=0;b<c.blocks.size();++b) {
            Definitions defs;
            for (auto pred:c.blocks[b].predecessors) for (unsigned reg=0;reg<32;++reg) defs[reg].insert(exit[pred][reg].begin(),exit[pred][reg].end());
            entry[b]=defs;
            for (Id pc=c.blocks[b].begin;pc<c.blocks[b].end;++pc) for (auto reg:backend::access(p.code[pc]).defs) defs[reg]={pc};
            if (exit[b]!=defs) { exit[b]=std::move(defs); changed=true; }
        }
    }
    for (Id b=0;b<c.blocks.size();++b) {
        auto defs=entry[b]; std::array<std::set<Id>,32> readers;
        std::optional<Id> effect;
        for (Id pc=c.blocks[b].begin;pc<c.blocks[b].end;++pc) {
            const auto& i=p.code[pc]; auto a=backend::access(i); std::set<Id> dependencies;
            for (auto reg:a.uses) dependencies.insert(defs[reg].begin(),defs[reg].end());
            for (auto reg:a.defs) { dependencies.insert(defs[reg].begin(),defs[reg].end()); dependencies.insert(readers[reg].begin(),readers[reg].end()); }
            if (a.barrier||a.may_trap) { if (effect) dependencies.insert(*effect); effect=pc; }
            // A self edge here denotes a loop-carried dependence, not a
            // same-iteration read. Preserve it instead of pretending this is a DAG.
            r.graph.nodes[pc]={std::string(metail::name(i.op)),{dependencies.begin(),dependencies.end()},i.immediate,a.barrier||a.may_trap};
            for (auto reg:a.uses) readers[reg].insert(pc);
            for (auto reg:a.defs) { defs[reg]={pc}; readers[reg].clear(); }
        }
    }
    for (Id n=0;n<p.size();++n) if (r.graph.nodes[n].effect) r.graph.outputs.push_back(n);
    return r;
}
} // namespace metachain::graph
