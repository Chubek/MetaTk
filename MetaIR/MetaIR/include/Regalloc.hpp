#pragma once
#include "../regalloc/LinearIR.hpp"
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace metachain::regalloc {
struct Result {
    // nullopt means spilled. Spill slots are abstract; rewriting is caller-owned.
    std::vector<std::optional<unsigned>> registers;
    std::map<backend::Id,std::size_t> spills;
    std::vector<backend::Diagnostic> diagnostics;
    explicit operator bool() const { return diagnostics.empty(); }
};
inline bool valid(const InterferenceGraph& g,const Result& r,unsigned colors) {
    if (!r||!verify(g,colors).empty()||r.registers.size()!=g.neighbors.size()) return false;
    for (backend::Id n=0;n<g.neighbors.size();++n) {
        if (r.registers[n]) {
            if (*r.registers[n]>=colors||r.spills.count(n)) return false;
            for (auto m:g.neighbors[n]) if (r.registers[m]==r.registers[n]) return false;
        } else if (!r.spills.count(n)) return false;
    }
    for (auto [n,c]:g.precolored) if (r.registers[n]!=c) return false;
    return true;
}
inline Result color(const InterferenceGraph& g,unsigned colors) {    Result r; r.diagnostics=verify(g,colors); if (!r) return r;
    const auto size=g.neighbors.size(); r.registers.resize(size);
    std::vector<bool> removed(size); for (auto [n,c]:g.precolored) { removed[n]=true; r.registers[n]=c; }
    std::vector<backend::Id> stack;
    while (stack.size()+g.precolored.size()<size) {
        backend::Id chosen=size; double cheapest=std::numeric_limits<double>::max();
        for (backend::Id n=0;n<size;++n) if (!removed[n]) {
            std::size_t degree=0;
            for (auto m:g.neighbors[n]) if (!removed[m]||g.precolored.count(m)) ++degree;
            if (degree<colors) { chosen=n; break; }
            const auto cost=(g.spill_cost.empty()?1.0:g.spill_cost[n])/double(degree+1);
            if (chosen==size||cost<cheapest) { cheapest=cost; chosen=n; }
        }
        removed[chosen]=true; stack.push_back(chosen);
    }
    while (!stack.empty()) {
        auto n=stack.back(); stack.pop_back(); std::set<unsigned> used;
        for (auto m:g.neighbors[n]) if (r.registers[m]) used.insert(*r.registers[m]);
        for (unsigned c=0;c<colors;++c) if (!used.count(c)) { r.registers[n]=c; break; }
        if (!r.registers[n]) r.spills[n]=r.spills.size();
    }
    return r;
}
struct AllocStats {
    std::size_t values=0, colored=0, spilled=0, moves=0;
    double density=0.0;
};
inline AllocStats alloc_stats(const InterferenceGraph& g,const Result& r) {
    AllocStats s; s.values=g.neighbors.size(); s.moves=g.moves.size();
    s.spilled=r.spills.size();
    s.colored=s.values>s.spilled?s.values-s.spilled:0;
    std::size_t edges=0;
    for (const auto& n:g.neighbors) edges+=n.size();
    edges/=2;
    s.density=s.values>1?double(2*edges)/double(s.values*(s.values-1)):0.0;
    return s;
}
inline std::string dump_allocation(const InterferenceGraph& g,const Result& r,unsigned colors) {
    std::ostringstream out;
    auto s=alloc_stats(g,r);
    out << "allocation colors=" << colors << " values=" << s.values << " colored=" << s.colored << " spilled=" << s.spilled << "\n";
    for (backend::Id n=0;n<r.registers.size();++n) {
        out << "  v" << n << " -> ";
        if (r.registers[n]) out << "r" << *r.registers[n];
        else {
            auto it=r.spills.find(n);
            out << (it==r.spills.end()?"<unassigned>":"spill#" + std::to_string(it->second));
        }
        auto pre=g.precolored.find(n);
        if (pre!=g.precolored.end()) out << " (fixed r" << pre->second << ")";
        out << "\n";
    }
    for (const auto& d:r.diagnostics) out << "  diagnostic: " << d.message << " @v" << d.node << "\n";
    return out.str();
}
Result graph_coloring(const InterferenceGraph&,unsigned colors);
Result briggs(const InterferenceGraph&,unsigned colors);
Result linear_scan(const std::vector<Interval>&,unsigned colors);
} // namespace metachain::regalloc
