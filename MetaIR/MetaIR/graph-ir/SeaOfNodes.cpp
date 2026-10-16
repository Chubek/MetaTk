#include "GraphIR.hpp"

namespace metachain::graph {
Analysis sea_of_nodes(const metail::Program& p) {
    auto r=vsdg(p); if (!r) return r;
    auto c=cfg(p); std::vector<Id> regions(c.blocks.size());
    for (Id b=0;b<regions.size();++b) { regions[b]=r.graph.nodes.size(); r.graph.nodes.push_back({"region",{},0,true}); }
    const Id base=regions.size()*32;
    for (Id b=0;b<regions.size();++b) {
        for (auto pred:c.blocks[b].predecessors) r.graph.nodes[regions[b]].inputs.push_back(base+c.blocks[pred].end-1);
        for (Id pc=c.blocks[b].begin;pc<c.blocks[b].end;++pc) r.graph.nodes[base+pc].inputs.push_back(regions[b]);
    }
    return r;
}
} // namespace metachain::graph
