#pragma once
#include "GraphIR.hpp"

namespace metachain::graph {
// Single-entry traces without tail duplication: never absorb a side entrance.
inline std::vector<std::vector<Id>> superblocks(const CFG& c) {
    if (!c) return {};
    std::vector<std::vector<Id>> result; std::vector<bool> used(c.blocks.size());
    for (Id start=0;start<c.blocks.size();++start) if (!used[start]) {
        std::vector<Id> trace; Id b=start;
        while (!used[b]) {
            used[b]=true; trace.push_back(b); std::optional<Id> next;
            for (auto s:c.blocks[b].successors) if (!used[s]&&c.blocks[s].predecessors.size()==1) { next=s; break; }
            if (!next) break;
            b=*next;
        }
        result.push_back(std::move(trace));
    }
    return result;
}
} // namespace metachain::graph
