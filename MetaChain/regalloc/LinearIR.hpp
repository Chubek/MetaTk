#pragma once
#include "GraphIR.hpp"
#include <cstddef>
#include <optional>
#include <set>
#include <vector>

namespace metachain::regalloc {
struct Interval {
    backend::Id value=0;
    std::size_t begin=0,end=0; // half-open; empty intervals are invalid
};
struct Liveness {
    std::vector<std::set<unsigned>> in,out;
    InterferenceGraph interference;
    std::vector<Interval> intervals;
    std::vector<backend::Diagnostic> diagnostics;
    explicit operator bool() const { return diagnostics.empty(); }
};
inline Liveness liveness(const metail::Program& p) {
    Liveness r;
    for (const auto& e:metail::verify(p)) r.diagnostics.push_back({e.message,e.instruction});
    if (!r) return r;
    r.in.resize(p.size()); r.out.resize(p.size()); r.interference.neighbors.resize(32);
    std::vector<backend::Access> a; for (auto i:p.code) a.push_back(backend::access(i));
    bool changed=true;
    while (changed) {
        changed=false;
        for (std::size_t pc=p.size();pc-->0;) {
            std::set<unsigned> out;
            auto merge=[&](std::size_t next){ if (next<p.size()) out.insert(r.in[next].begin(),r.in[next].end()); };
            const auto& i=p.code[pc];
            if (metail::branch(i.op)) merge(static_cast<std::size_t>(i.immediate));
            if (i.op==metail::Op::Ret) for (unsigned n=0;n<32;++n) out.insert(n);
            if (i.op!=metail::Op::Jmp&&i.op!=metail::Op::Ret&&i.op!=metail::Op::Halt) merge(pc+1);
            auto in=out; for (auto d:a[pc].defs) in.erase(d); in.insert(a[pc].uses.begin(),a[pc].uses.end());
            if (in!=r.in[pc]||out!=r.out[pc]) { changed=true; r.in[pc]=std::move(in); r.out[pc]=std::move(out); }
        }
    }
    std::array<std::optional<std::size_t>,32> first{},last{};
    for (std::size_t pc=0;pc<p.size();++pc) {
        auto live=r.in[pc]; live.insert(r.out[pc].begin(),r.out[pc].end());
        live.insert(a[pc].defs.begin(),a[pc].defs.end()); live.insert(a[pc].uses.begin(),a[pc].uses.end());
        for (auto n:live) { if (!first[n]) first[n]=pc; last[n]=pc+1; }
        // Values live at entry interfere even if neither has an explicit def.
        for (auto n:r.in[pc]) for (auto m:r.in[pc]) r.interference.add_edge(n,m);
        for (auto n:a[pc].defs) for (auto m:r.out[pc]) r.interference.add_edge(n,m);
        if (p.code[pc].op==metail::Op::Mov) r.interference.moves.push_back({p.code[pc].a,p.code[pc].b});
    }
    for (unsigned n=0;n<32;++n) if (first[n]) r.intervals.push_back({n,*first[n],*last[n]});
    return r;
}
} // namespace metachain::regalloc
