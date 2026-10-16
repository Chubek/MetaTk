#include "GraphIR.hpp"

namespace metachain::graph {
Analysis vsdg(const metail::Program& p) {
    auto s=gssa(p); Analysis r{std::move(s.graph),std::move(s.diagnostics)}; if (!r) return r;
    // One state token per block, merged on CFG edges, threads all effects.
    std::vector<Id> entry(s.control.blocks.size()),state(entry.size());
    for (Id b=0;b<entry.size();++b) { entry[b]=r.graph.nodes.size(); r.graph.nodes.push_back({"state.phi",{},0,true}); }
    const Id base=s.control.blocks.size()*32;
    for (Id b=0;b<entry.size();++b) {
        state[b]=entry[b];
        for (Id pc=s.control.blocks[b].begin;pc<s.control.blocks[b].end;++pc) if (r.graph.nodes[base+pc].effect) {
            r.graph.nodes[base+pc].inputs.push_back(state[b]); state[b]=base+pc;
        }
    }
    for (Id b=0;b<entry.size();++b) {
        for (auto pred:s.control.blocks[b].predecessors) r.graph.nodes[entry[b]].inputs.push_back(state[pred]);
        if (b==0||s.control.blocks[b].predecessors.empty()) {
            auto initial=r.graph.nodes.size(); r.graph.nodes[entry[b]].inputs.insert(r.graph.nodes[entry[b]].inputs.begin(),initial);
            r.graph.nodes.push_back({"state.start",{},0,true});
        }
    }
    return r;
}
} // namespace metachain::graph
