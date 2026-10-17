// Tables that the generated run-times (portable C and native x86-64) read from
// a unit: the type descriptors. The built-in names live in Runtime.h.
#include "qscript/src/Runtime.h"
#include "qscript/src/Types.h"

namespace qscript::detail {
Result<bool> needs_full_runtime(const Unit& unit) {
    bool full = false;
    if (!uses_builtins(unit.program)) return {false, {}};
    for (const auto& i : unit.program.code) {
        if (i.op != metail::Op::Host || !is_builtin_id(i.immediate)) continue;
        const auto b = static_cast<Builtin>(i.immediate - builtin_base);
        if (b < Builtin::Die || b > Builtin::PrintValue)
            return {std::nullopt, {{metachain::Stage::Lower, "unknown QScript built-in", {}, 0, 0, 0}}};
        bool standalone = false;
        for (const auto& c : runtime_builtins) standalone = standalone || c.id == b;
        full = full || !standalone;
    }
    for (const auto& name : unit.types) {
        auto type = parse_type(name);
        if (!type) return {std::nullopt, {{metachain::Stage::Lower, "invalid type descriptor " + name, {}, 0, 0, 0}}};
        while (type->is_list()) { auto element = type->element(); *type = std::move(element); }
        full = full || type->kind == Type::Kind::Grammar || type->kind == Type::Kind::Match;
    }
    return {full, {}};
}
namespace {
// Index of the table entry for t. Element types are appended after the
// descriptors, so the descriptor numbers of the unit stay valid.
int encode(std::vector<RunTimeType>& table, const Type& t) {
    const int elem = t.is_list() ? encode(table, t.element()) : -1;
    table.push_back({static_cast<int>(t.kind), elem});
    return static_cast<int>(table.size()) - 1;
}
} // namespace

Result<std::vector<RunTimeType>> run_time_types(const Unit& unit) {
    std::vector<RunTimeType> table(unit.types.size(), RunTimeType{0, -1});
    for (std::size_t i = 0; i < unit.types.size(); ++i) {
        auto t = parse_type(unit.types[i]);
        if (!t) return {std::nullopt, {{metachain::Stage::Lower, "internal: invalid type descriptor " + unit.types[i], {}, 0, 0, 0}}};
        const int elem = t->is_list() ? encode(table, t->element()) : -1;
        table[i] = {static_cast<int>(t->kind), elem};
    }
    return {std::move(table), {}};
}
} // namespace qscript::detail
