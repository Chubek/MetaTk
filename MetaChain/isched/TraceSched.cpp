#include "metachain/ISched.hpp"

namespace metachain::isched {
namespace {
// Total outgoing weight of a block; used to rank trace heads.
double block_heat(const TraceBlock& b) {
    double total=0;
    for (auto w:b.weights) total+=w;
    return total;
}
} // namespace
// Trace scheduling: greedily extend each trace along the hottest unused
// successor. Forms single-entry traces (side entrances start new traces);
// no code motion, compensation code, or tail duplication is performed here.
std::vector<std::vector<Id>> trace_schedule(const std::vector<TraceBlock>& blocks) {
    for (const auto& b:blocks) {
        if (b.weights.size()!=b.successors.size()) throw std::invalid_argument("trace weight count mismatch");
        for (auto s:b.successors) if (s>=blocks.size()) throw std::invalid_argument("trace successor outside CFG");
        for (auto w:b.weights) if (!(w>=0)||w>std::numeric_limits<double>::max()) throw std::invalid_argument("invalid trace weight");
    }
    // Start new traces from the hottest blocks first for stable output.
    std::vector<Id> heads(blocks.size());
    std::iota(heads.begin(),heads.end(),Id{0});
    std::stable_sort(heads.begin(),heads.end(),[&](Id a,Id b){return block_heat(blocks[a])>block_heat(blocks[b]);});
    std::vector<std::vector<Id>> traces; std::vector<bool> used(blocks.size());
    for (auto start:heads) if (!used[start]) {
        std::vector<Id> trace; Id b=start;
        while (!used[b]) {
            used[b]=true; trace.push_back(b); std::optional<Id> next; double weight=-1;
            for (Id k=0;k<blocks[b].successors.size();++k)
                if (!used[blocks[b].successors[k]]&&blocks[b].weights[k]>weight) {
                    next=blocks[b].successors[k]; weight=blocks[b].weights[k];
                }
            if (!next) break;
            b=*next;
        }
        traces.push_back(std::move(trace));
    }
    return traces;
}
} // namespace metachain::isched
