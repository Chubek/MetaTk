#include "metachain/ISched.hpp"
namespace metachain::isched {
std::vector<std::vector<Id>> trace_schedule(const std::vector<TraceBlock>& blocks) {
    for (const auto& b:blocks) {
        if (b.weights.size()!=b.successors.size()) throw std::invalid_argument("trace weight count mismatch");
        for (auto s:b.successors) if (s>=blocks.size()) throw std::invalid_argument("trace successor outside CFG");
        for (auto w:b.weights) if (!(w>=0)||w>std::numeric_limits<double>::max()) throw std::invalid_argument("invalid trace weight");
    }
    std::vector<std::vector<Id>> traces; std::vector<bool> used(blocks.size());
    for (Id start=0;start<blocks.size();++start) if (!used[start]) {
        std::vector<Id> trace; Id b=start;
        while (!used[b]) {
            used[b]=true; trace.push_back(b); std::optional<Id> next; double weight=-1;
            for (Id k=0;k<blocks[b].successors.size();++k) if (!used[blocks[b].successors[k]]&&blocks[b].weights[k]>weight) { next=blocks[b].successors[k]; weight=blocks[b].weights[k]; }
            if (!next) break;
            b=*next;
        }
        traces.push_back(std::move(trace));
    }
    return traces;
}
} // namespace metachain::isched
