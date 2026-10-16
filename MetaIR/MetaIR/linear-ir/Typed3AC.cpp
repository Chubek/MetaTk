#include "LinearIR.hpp"

namespace metachain::linear {
// Typed three-address code: every register operand carries I64, every branch
// target carries Label. Types are checked on construction so mistyped IR
// fails fast instead of reaching the MetaIL verifier.
Code typed_three_address(const metail::Program& p) {
    auto r=quadruples(p);
    if (!r) return r;
    for (std::size_t pc=0;pc<r.instructions.size();++pc) {
        const auto& q=r.instructions[pc];
        auto expect=[&](const Operand& o,Type t,const char* what) {
            if (o.kind!=Operand::Kind::None&&o.type!=t)
                r.diagnostics.push_back({std::string("typed 3AC expects ")+what,pc});
        };
        if (metail::binary(q.op)) {
            expect(q.result,Type::I64,"i64 result"); expect(q.first,Type::I64,"i64 operand"); expect(q.second,Type::I64,"i64 operand");
        } else if (q.op==metail::Op::Mov) {
            expect(q.result,Type::I64,"i64 result"); expect(q.first,Type::I64,"i64 operand");
        } else if (q.op==metail::Op::Const) {
            expect(q.result,Type::I64,"i64 result"); expect(q.first,Type::I64,"i64 immediate");
        }
        if (metail::branch(q.op)) expect(q.second,Type::Label,"label target");
    }
    return r;
}
} // namespace metachain::linear
