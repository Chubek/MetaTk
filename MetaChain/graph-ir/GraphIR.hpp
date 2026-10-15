#pragma once
#include "metachain/Backend.hpp"
#include <array>
#include <iterator>
#include <optional>
#include <set>
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
// VSDG/sea-of-nodes preserve explicit control and effect dependencies.
Analysis vsdg(const metail::Program&);
Analysis sea_of_nodes(const metail::Program&);
std::vector<std::set<Id>> dominators(const CFG&);
} // namespace metachain::graph
