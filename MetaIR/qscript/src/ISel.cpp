// Native (Linux x86-64) selection for QScript. Integer programs are the MetaIL
// lowering itself. Programs that use strings and lists also get the built-in
// runtime of NativeRuntime.cpp. Grammar/match programs use a C-ABI entry and
// statically link the existing parser runtime; their control flow is native.
#include "qscript/src/Runtime.h"
#include "qscript/QScript.h"

namespace qscript {
Result<std::string> select(const metail::Program& p, Options o) {
    if (detail::uses_builtins(p)) return {std::nullopt,{{metachain::Stage::Lower,detail::builtins_need_unit,{},0,0,0}}};
    // MetaIL already encodes the concrete integer operations; use the existing
    // x86-64 lowering, including its runtime traps and execution limits.
    return metachain::Toolchain(o.toolchain).native_source(p);
}

Result<std::string> select(const Unit& unit, Options o) {
    const auto& p = unit.program;
    if (!detail::uses_builtins(p)) return select(p, o);
    auto full = detail::needs_full_runtime(unit);
    if (!full) return {std::nullopt, std::move(full.diagnostics)};
    for (const auto& i : p.code) {
        if (i.op == metail::Op::Host && !detail::is_builtin_id(i.immediate))
            return {std::nullopt,{{metachain::Stage::Lower,"native output cannot embed process-local host bindings",{},0,0,0}}};
    }
    if (*full) return detail::linked_native_source(unit, o);
    auto types = detail::run_time_types(unit);
    if (!types) return {std::nullopt, std::move(types.diagnostics)};
    metachain::NativeRuntime runtime;
    // The state block is mapped first; everything else reads it through r13.
    runtime.prologue = "call qs_rt_init\n";
    runtime.builtin = "qs_rt_builtin";
    runtime.text = detail::native_runtime_text(unit.strings, *types, o.max_heap_objects, o.max_heap_bytes);
    return metachain::Toolchain(o.toolchain).native_source(p, runtime);
}
} // namespace qscript
