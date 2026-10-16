#include "metachain/ISched.hpp"

namespace metachain::isched {
// Fill pre-reserved branch delay slots with safe earlier instructions.
// The input must reserve exactly `slots` nops after every branch; filled
// slots keep their relative order. Trapping, barrier, and branch candidates
// never move. Straight-line regions only: no interior branch targets.
DelayResult delay_slots(const std::vector<DelayInstruction>& code,unsigned slots) {
    DelayResult r;
    for (Id n=0;n<code.size();++n) r.order.push_back(code[n].nop?std::nullopt:std::optional<Id>(n));
    auto motion_safe=[&](Id candidate,Id branch) {
        const auto& a=code[candidate].access;
        for (Id crossed=candidate+1;crossed<=branch;++crossed) if (r.order[crossed]) {
            const auto& x=code[*r.order[crossed]].access;
            if (backend::intersects(a.defs,x.uses)||backend::intersects(a.uses,x.defs)||backend::intersects(a.defs,x.defs))
                return false;
        }
        return true;
    };
    for (Id b=0;b<code.size();++b) if (code[b].branch) {
        if (slots>code.size()-b-1) { r.diagnostics.push_back({"missing reserved delay slots",b}); return r; }
        for (unsigned s=1;s<=slots;++s) if (!code[b+s].nop) { r.diagnostics.push_back({"delay slots must initially be nops",b+s}); return r; }
        unsigned filled=0;
        for (Id candidate=b;candidate-->0&&filled<slots;) {
            if (code[candidate].branch||code[candidate].access.barrier||code[candidate].access.may_trap) break;
            if (!r.order[candidate]) continue;
            // Candidates are visited backwards; place them right-to-left to
            // preserve their original relative order inside the delay slots.
            if (motion_safe(candidate,b)) { r.order[b+slots-filled]=candidate; r.order[candidate].reset(); ++filled; }
        }
        b+=slots;
    }
    return r;
}
} // namespace metachain::isched
