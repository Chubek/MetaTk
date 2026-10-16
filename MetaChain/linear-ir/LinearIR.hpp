#pragma once
#include "metachain/Backend.hpp"
#include <sstream>
#include <string>
#include <vector>

namespace metachain::linear {
enum class Type { None, I64, Label };
struct Operand {
    enum class Kind { None, Register, Immediate, Label } kind=Kind::None;
    std::int64_t value=0;
    Type type=Type::None;
    bool operator==(const Operand&) const = default;
};
struct Quadruple {
    metail::Op op=metail::Op::Nop;
    Operand result, first, second;
    bool operator==(const Quadruple&) const = default;
};
struct Code {
    std::vector<Quadruple> instructions;
    std::vector<backend::Diagnostic> diagnostics;
    explicit operator bool() const { return diagnostics.empty(); }
};
Code quadruples(const metail::Program&);
Code typed_three_address(const metail::Program&);
Code untyped_three_address(const metail::Program&);
metail::Result to_metail(const Code&);
struct Triple {
    metail::Op op=metail::Op::Nop;
    Operand first, second;
    // Register destination is retained: triples are not implicitly SSA.
    Operand destination;
};
std::vector<Triple> triples(const Code&);
Code from_triples(const std::vector<Triple>&);
inline std::string dump_operand(const Operand& o) {
    std::ostringstream out;
    switch (o.kind) {
    case Operand::Kind::Register: out << "r" << o.value; break;
    case Operand::Kind::Immediate: out << "#" << o.value; break;
    case Operand::Kind::Label: out << "L" << o.value; break;
    default: out << "_"; break;
    }
    if (o.type==Type::I64) out << ":i64";
    else if (o.type==Type::Label) out << ":label";
    return out.str();
}
inline std::string dump_code(const Code& c) {
    std::ostringstream out;
    for (std::size_t pc=0;pc<c.instructions.size();++pc) {
        const auto& q=c.instructions[pc];
        out << pc << ": " << metail::name(q.op) << " " << dump_operand(q.result)
            << ", " << dump_operand(q.first) << ", " << dump_operand(q.second) << "\n";
    }
    for (const auto& d:c.diagnostics) out << "diagnostic: " << d.message << " @" << d.node << "\n";
    return out.str();
}
std::string pseudo_assembly(const Code&);
} // namespace metachain::linear
