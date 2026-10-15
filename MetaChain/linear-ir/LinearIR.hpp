#pragma once
#include "metachain/Backend.hpp"
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
std::string pseudo_assembly(const Code&);
} // namespace metachain::linear
