#include "GraphIR.hpp"

namespace metachain::graph {
CFG cfg(const metail::Program& p) {
    CFG r;
    for (const auto& e:metail::verify(p)) r.diagnostics.push_back({e.message,e.instruction});
    if (!r) return r;
    std::set<Id> leaders{0,p.size()};
    for (Id pc=0;pc<p.size();++pc) {
        auto op=p.code[pc].op;
        if (metail::branch(op)) leaders.insert(static_cast<Id>(p.code[pc].immediate));
        if (metail::branch(op)||op==metail::Op::Ret||op==metail::Op::Halt) leaders.insert(pc+1);
    }
    r.block_at.resize(p.size());
    for (auto it=leaders.begin(),next=std::next(it);next!=leaders.end();++it,++next) {
        Id b=r.blocks.size(); r.blocks.push_back({*it,*next,{},{}});
        for (Id pc=*it;pc<*next;++pc) r.block_at[pc]=b;
    }
    for (Id b=0;b<r.blocks.size();++b) {
        auto& block=r.blocks[b]; const auto& i=p.code[block.end-1];
        auto add=[&](Id next){ auto& s=block.successors; if (std::find(s.begin(),s.end(),next)==s.end()) s.push_back(next); };
        if (metail::branch(i.op)) add(r.block_at[static_cast<Id>(i.immediate)]);
        if (i.op==metail::Op::Ret) {
            // Return addresses are dynamic; conservatively include all call continuations.
            for (Id pc=0;pc+1<p.size();++pc) if (p.code[pc].op==metail::Op::Call) add(r.block_at[pc+1]);
        } else if (i.op!=metail::Op::Jmp&&i.op!=metail::Op::Halt&&block.end<p.size()) add(r.block_at[block.end]);
    }
    for (Id b=0;b<r.blocks.size();++b) for (auto s:r.blocks[b].successors) r.blocks[s].predecessors.push_back(b);
    return r;
}
std::vector<std::set<Id>> dominators(const CFG& c) {
    const Id n=c.blocks.size(); if (!c||!n) return {};
    std::set<Id> reachable; std::vector<Id> work{0};
    while (!work.empty()) { auto b=work.back(); work.pop_back(); if (!reachable.insert(b).second) continue; for (auto s:c.blocks[b].successors) work.push_back(s); }
    std::vector<std::set<Id>> dom(n);
    for (Id b=0;b<n;++b) dom[b]=reachable.count(b)?reachable:std::set<Id>{b};
    dom[0]={0}; bool changed=true;
    while (changed) {
        changed=false;
        for (Id b=1;b<n;++b) if (reachable.count(b)) {
            auto d=reachable;
            for (auto pred:c.blocks[b].predecessors) if (reachable.count(pred)) {
                std::set<Id> intersection; std::set_intersection(d.begin(),d.end(),dom[pred].begin(),dom[pred].end(),std::inserter(intersection,intersection.end())); d=std::move(intersection);
            }
            d.insert(b); if (d!=dom[b]) { dom[b]=std::move(d); changed=true; }
        }
    }
    return dom;
}
} // namespace metachain::graph
