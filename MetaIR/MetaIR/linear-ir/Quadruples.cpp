#include "LinearIR.hpp"

namespace metachain::linear {
Code quadruples(const metail::Program& p) {
    Code r; for (const auto& e:metail::verify(p)) r.diagnostics.push_back({e.message,e.instruction}); if (!r) return r;
    auto reg=[](unsigned n){return Operand{Operand::Kind::Register,static_cast<std::int64_t>(n),Type::I64};};
    for (auto i:p.code) {
        Quadruple q; q.op=i.op;
        if (metail::binary(i.op)) { q.result=reg(i.a); q.first=reg(i.b); q.second=reg(i.c); }
        else if (i.op==metail::Op::Mov) { q.result=reg(i.a); q.first=reg(i.b); }
        else if (i.op==metail::Op::Const) { q.result=reg(i.a); q.first={Operand::Kind::Immediate,i.immediate,Type::I64}; }
        else if (i.op==metail::Op::Pop) q.result=reg(i.a);
        else if (metail::registers(i.op)) q.first=reg(i.a);
        if (metail::branch(i.op)) q.second={Operand::Kind::Label,i.immediate,Type::Label};
        r.instructions.push_back(q);
    }
    return r;
}
metail::Result to_metail(const Code& code) {
    metail::Result r;
    for (const auto& e:code.diagnostics) r.diagnostics.push_back({e.message,e.node,{}});
    for (std::size_t pc=0;pc<code.instructions.size();++pc) {
        const auto& q=code.instructions[pc]; metail::Instruction i; i.op=q.op;
        auto read=[&](const Operand& o,Operand::Kind k)->std::int64_t {
            if (o.kind!=k || (k==Operand::Kind::Register&&(o.value<0||o.value>=32))) r.diagnostics.push_back({"invalid linear IR operand",pc,{}});
            return o.value;
        };
        if (metail::binary(i.op)) { i.a=static_cast<std::uint8_t>(read(q.result,Operand::Kind::Register)); i.b=static_cast<std::uint8_t>(read(q.first,Operand::Kind::Register)); i.c=static_cast<std::uint8_t>(read(q.second,Operand::Kind::Register)); }
        else if (i.op==metail::Op::Mov) { i.a=static_cast<std::uint8_t>(read(q.result,Operand::Kind::Register)); i.b=static_cast<std::uint8_t>(read(q.first,Operand::Kind::Register)); }
        else if (i.op==metail::Op::Const) { i.a=static_cast<std::uint8_t>(read(q.result,Operand::Kind::Register)); i.immediate=read(q.first,Operand::Kind::Immediate); }
        else if (i.op==metail::Op::Pop) i.a=static_cast<std::uint8_t>(read(q.result,Operand::Kind::Register));
        else if (metail::registers(i.op)) i.a=static_cast<std::uint8_t>(read(q.first,Operand::Kind::Register));
        if (metail::branch(i.op)) i.immediate=read(q.second,Operand::Kind::Label);
        r.program.code.push_back(i);
    }
    // Canonical shape check also catches silently ignored extra operands/types.
    auto errors=metail::verify(r.program); r.diagnostics.insert(r.diagnostics.end(),errors.begin(),errors.end());
    if (r.diagnostics.empty()) {
        auto canonical=quadruples(r.program);
        for (std::size_t pc=0;pc<code.instructions.size();++pc) {
            auto q=code.instructions[pc]; auto expected=canonical.instructions[pc];
            auto type_ok=[](const Operand& value,const Operand& shape){return value.type==Type::None||value.type==shape.type;};
            if (!type_ok(q.result,expected.result)||!type_ok(q.first,expected.first)||!type_ok(q.second,expected.second))
                r.diagnostics.push_back({"invalid linear IR operand type",pc,{}});
            auto normalize=[](Operand& o){o.type=Type::None;};
            normalize(q.result); normalize(q.first); normalize(q.second);
            normalize(expected.result); normalize(expected.first); normalize(expected.second);
            if (q!=expected) r.diagnostics.push_back({"noncanonical linear IR operands",pc,{}});
        }
    }
    return r;
}
} // namespace metachain::linear
