#include "qscript/QScript.h"
namespace qscript {
Result<metail::Program> schedule(metail::Program p,Options o) {
    auto checked=verify_ir(std::move(p),o); if (!checked) return checked;
    p=std::move(*checked);
    // Reorder only within basic blocks, preserving instruction counts and
    // source maps so absolute branch targets remain valid.
    metachain::isched::Options machine;
    machine.issue_width=o.backend.issue_width?o.backend.issue_width:1;
    std::set<std::size_t> leaders{0,p.size()};
    for (std::size_t pc=0;pc<p.size();++pc) {
        const auto& i=p.code[pc];
        if (metail::branch(i.op)) leaders.insert(static_cast<std::size_t>(i.immediate));
        if (metail::branch(i.op)||i.op==metail::Op::Ret||i.op==metail::Op::Halt) leaders.insert(pc+1);
    }
    for (auto it=leaders.begin(),end=std::next(it);end!=leaders.end();++it,++end) {
        // Dependence construction is quadratic. Large blocks retain their
        // already valid input order instead of allocating an unbounded DAG.
        if (*end-*it>256) continue;
        std::vector<metail::Instruction> code(p.code.begin()+*it,p.code.begin()+*end);
        const auto tasks=metachain::isched::dependencies(code);
        metachain::isched::Result scheduled;
        // Dispatch without linking the Backend library: cycle/static are
        // specializations of the header-inline list scheduler.
        if (o.backend.scheduler==Scheduler::Static) {
            auto serial=machine; serial.issue_width=1; serial.resources.clear();
            scheduled=metachain::isched::list_schedule(tasks,serial,false);
        } else {
            scheduled=metachain::isched::list_schedule(tasks,machine,o.backend.scheduler==Scheduler::List);
        }
        // Dependencies are acyclic by construction, but never trust a
        // reordering that fails validation; retain the verified input order.
        if (!scheduled||scheduled.order.size()!=code.size()||!metachain::isched::valid(tasks,scheduled,machine)) continue;
        std::vector<metavm::SourceLocation> locations;
        if (!p.source_map.empty()) locations.assign(p.source_map.begin()+*it,p.source_map.begin()+*end);
        for (std::size_t n=0;n<code.size();++n) {
            p.code[*it+n]=code[scheduled.order[n]];
            if (!locations.empty()) p.source_map[*it+n]=locations[scheduled.order[n]];
        }
    }
    return verify_ir(std::move(p),o);
}
} // namespace qscript
