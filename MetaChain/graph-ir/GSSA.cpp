#include "GraphIR.hpp"

namespace metachain::graph {
SSA gssa(const metail::Program& p) {
    SSA r; r.control=cfg(p); r.diagnostics=r.control.diagnostics; if (!r) return r;
    for (Id pc=0;pc<p.size();++pc) if (p.code[pc].op==metail::Op::Call||p.code[pc].op==metail::Op::Ret) {
        r.diagnostics.push_back({"SSA requires intraprocedural IL (call/ret unsupported)",pc}); return r;
    }
    // Block arguments are phi nodes even for a single predecessor. Keeping
    // them explicit makes loops and unreachable components well-defined.
    std::vector<std::array<Id,32>> args(r.control.blocks.size()),exits(args.size());
    for (Id b=0;b<args.size();++b) for (unsigned reg=0;reg<32;++reg) {
        args[b][reg]=r.graph.nodes.size(); r.graph.nodes.push_back({"phi",{},static_cast<std::int64_t>(reg),false});
    }
    const Id base=r.graph.nodes.size(); r.graph.nodes.resize(base+p.size());
    for (Id b=0;b<args.size();++b) {
        auto values=args[b]; std::optional<Id> effect;
        for (Id pc=r.control.blocks[b].begin;pc<r.control.blocks[b].end;++pc) {
            const auto& i=p.code[pc]; auto a=backend::access(i); auto& n=r.graph.nodes[base+pc];
            n={std::string(metail::name(i.op)),{},i.immediate,a.barrier||a.may_trap};
            // Preserve operand order and multiplicity for value graphs.
            if (metail::binary(i.op)) n.inputs={values[i.b],values[i.c]};
            else if (i.op==metail::Op::Mov) n.inputs={values[i.b]};
            else for (auto reg:a.uses) n.inputs.push_back(values[reg]);
            if (n.effect&&effect) n.inputs.push_back(*effect);
            if (n.effect) { effect=base+pc; r.graph.outputs.push_back(base+pc); }
            for (auto reg:a.defs) values[reg]=base+pc;
        }
        exits[b]=values;
    }
    for (Id b=0;b<args.size();++b) for (unsigned reg=0;reg<32;++reg) {
        auto id=args[b][reg]; auto& n=r.graph.nodes[id];
        for (auto pred:r.control.blocks[b].predecessors) n.inputs.push_back(exits[pred][reg]);
        if (b==0||n.inputs.empty()) {
            auto zero=r.graph.nodes.size();
            // push_back may invalidate n; update it before growing the vector.
            n.inputs.insert(n.inputs.begin(),zero);
            r.graph.nodes.push_back({"const",{},0,false});
        }
        r.phis.push_back({b,reg,r.graph.nodes[id].inputs,id});
    }
    return r;
}
} // namespace metachain::graph
