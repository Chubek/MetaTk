#include "metachain/Regalloc.hpp"
#include <algorithm>
#include <set>
namespace metachain::regalloc {
Result linear_scan(const std::vector<Interval>& input,unsigned colors) {
    Result r;
    if (!colors) { r.diagnostics.push_back({"register count must be positive",0}); return r; }
    // Values need not be dense, but bound allocation for malformed IDs.
    backend::Id count=0; std::set<backend::Id> seen;
    for (auto i:input) {
        if (i.begin>=i.end||i.value>=1000000||!seen.insert(i.value).second) { r.diagnostics.push_back({"invalid or duplicate live interval",i.value}); return r; }
        count=std::max(count,i.value+1);
    }
    r.registers.resize(count);
    auto intervals=input; std::stable_sort(intervals.begin(),intervals.end(),[](auto a,auto b){return a.begin<b.begin;});
    std::vector<Interval> active; std::set<unsigned> free;
    // No more than input.size() registers can be simultaneously useful.
    for (std::size_t c=0;c<std::min<std::size_t>(colors,input.size());++c) free.insert(static_cast<unsigned>(c));
    for (auto current:intervals) {
        for (auto it=active.begin();it!=active.end();) {
            if (it->end<=current.begin) {
                // Active intervals always hold a register; guard anyway so a
                // future caller change cannot turn expiry into UB.
                if (r.registers[it->value]) free.insert(*r.registers[it->value]);
                it=active.erase(it);
            } else ++it;
        }
        if (!free.empty()) { r.registers[current.value]=*free.begin(); free.erase(free.begin()); active.push_back(current); }
        else {
            auto victim=std::max_element(active.begin(),active.end(),[](auto a,auto b){return a.end<b.end;});
            if (victim->end>current.end) {
                r.registers[current.value]=r.registers[victim->value]; r.registers[victim->value].reset();
                r.spills[victim->value]=r.spills.size(); *victim=current;
            } else r.spills[current.value]=r.spills.size();
        }
    }
    return r;
}
} // namespace metachain::regalloc
