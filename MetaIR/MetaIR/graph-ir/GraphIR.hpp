#pragma once
#include "metachain/Backend.hpp"
#include <array>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace metachain::graph {
using backend::Id;
using backend::Graph;
struct Block {
    Id begin=0, end=0; // half-open instruction interval
    std::vector<Id> successors, predecessors;
};
struct CFG {
    std::vector<Block> blocks;
    std::vector<Id> block_at;
    std::vector<backend::Diagnostic> diagnostics;
    explicit operator bool() const { return diagnostics.empty(); }
};
CFG cfg(const metail::Program&);
struct Analysis {
    Graph graph;
    std::vector<backend::Diagnostic> diagnostics;
    explicit operator bool() const { return diagnostics.empty(); }
};
// DFG is a dependence graph (RAW/WAR/WAW and ordered effects), not SSA.
Analysis dfg(const metail::Program&);
Analysis dag(const Graph&);
Analysis value_graph(const metail::Program&);
Analysis pdg(const metail::Program&);
// Program-dependence web: undirected connected components of a PDG.
std::vector<std::vector<Id>> pdw(const Graph&);
struct Phi { Id block=0; unsigned reg=0; std::vector<Id> definitions; Id result=0; };
struct SSA {
    Graph graph;
    CFG control;
    std::vector<Phi> phis;
    std::vector<backend::Diagnostic> diagnostics;
    explicit operator bool() const { return diagnostics.empty(); }
};
SSA gssa(const metail::Program&);
inline std::string dump_cfg(const CFG& c) {
    std::ostringstream out;
    out << "cfg blocks=" << c.blocks.size() << "\n";
    for (Id b=0;b<c.blocks.size();++b) {
        out << "  B" << b << " [" << c.blocks[b].begin << "," << c.blocks[b].end << ") succ{";
        for (std::size_t k=0;k<c.blocks[b].successors.size();++k) out << (k?",":"") << "B" << c.blocks[b].successors[k];
        out << "} pred{";
        for (std::size_t k=0;k<c.blocks[b].predecessors.size();++k) out << (k?",":"") << "B" << c.blocks[b].predecessors[k];
        out << "}\n";
    }
    for (const auto& d:c.diagnostics) out << "  diagnostic: " << d.message << " @%" << d.node << "\n";
    return out.str();
}
inline std::string dump_analysis(const Analysis& a) {
    std::ostringstream out;
    out << backend::dump(a.graph);
    for (const auto& d:a.diagnostics) out << "diagnostic: " << d.message << " @%" << d.node << "\n";
    return out.str();
}
// VSDG/sea-of-nodes preserve explicit control and effect dependencies.
Analysis vsdg(const metail::Program&);
Analysis sea_of_nodes(const metail::Program&);
std::vector<std::set<Id>> dominators(const CFG&);
} // namespace metachain::graph
