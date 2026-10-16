#ifndef METACHAIN_QSCRIPT_BACKEND_H
#define METACHAIN_QSCRIPT_BACKEND_H
// QScript backend configuration: instruction scheduling and optimization.
//
// The compile pipeline parses, lowers, optimizes, and block-schedules every
// program. These entry points expose that backend configuration to C callers
// and document the C++ surface. Scheduling never moves instructions across
// basic-block boundaries, so branch targets stay valid.
#include "qscript/QScript.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum qs_scheduler {
    QS_SCHED_LIST = 0,   /* critical-path list scheduling (default) */
    QS_SCHED_CYCLE = 1,  /* deterministic cycle-driven order */
    QS_SCHED_STATIC = 2  /* serial static baseline */
} qs_scheduler;

typedef struct qs_backend_options {
    int scheduler;          /* a qs_scheduler code; unknown codes are rejected */
    unsigned issue_width;   /* 0 means "keep current"; otherwise exact width */
    int optimize;           /* <0 keep, 0 disable, >0 enable MetaIL optimization */
} qs_backend_options;

qs_status qs_set_backend(qs_context*, const qs_backend_options*);
qs_status qs_set_optimize(qs_context*, int enable);
qs_status qs_backend_describe(const qs_context*, char* text, size_t capacity, size_t* length);

/* Read back the active configuration (optimize is reported as 0 or 1). */
qs_status qs_get_backend(const qs_context*, qs_backend_options*);
/* Select a scheduler by name: "list", "cycle" or "static". */
qs_status qs_set_scheduler_name(qs_context*, const char* name);
/* Name of a qs_scheduler code, or NULL for unknown codes. */
const char* qs_scheduler_name(int scheduler);
/* Per-program backend report: basic blocks, the critical path of each
   scheduled block and the resulting makespan. Buffer owned by the context. */
qs_status qs_backend_report(qs_context*, const qs_program*, const char** text, size_t* length);

#ifdef __cplusplus
}

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace qscript::backend {
// Human-readable description of the active backend configuration.
inline std::string describe(const Options& o) {
    std::string scheduler = "list";
    if (o.backend.scheduler == Scheduler::Cycle) scheduler = "cycle";
    else if (o.backend.scheduler == Scheduler::Static) scheduler = "static";
    return "scheduler=" + scheduler +
        " issue_width=" + std::to_string(o.backend.issue_width) +
        " optimize=" + (o.toolchain.optimize ? "on" : "off");
}
// Parse a scheduler name ("list", "cycle", "static").
inline std::optional<Scheduler> scheduler_from_name(std::string_view n) {
    if (n == "list") return Scheduler::List;
    if (n == "cycle") return Scheduler::Cycle;
    if (n == "static") return Scheduler::Static;
    return std::nullopt;
}
// Basic-block summary of a program as the scheduler sees it.
struct BlockReport { std::size_t begin = 0, end = 0, critical_path = 0, makespan = 0; bool scheduled = false; };
inline std::vector<BlockReport> report(const metail::Program& p, const Options& o) {
    std::set<std::size_t> leaders{0, p.size()};
    for (std::size_t pc = 0; pc < p.size(); ++pc) {
        const auto& i = p.code[pc];
        if (metail::branch(i.op)) leaders.insert(static_cast<std::size_t>(i.immediate));
        if (metail::branch(i.op) || i.op == metail::Op::Ret || i.op == metail::Op::Halt) leaders.insert(pc + 1);
    }
    std::vector<BlockReport> blocks;
    metachain::isched::Options machine;
    machine.issue_width = o.backend.issue_width ? o.backend.issue_width : 1;
    for (auto it = leaders.begin(), end = std::next(it); end != leaders.end(); ++it, ++end) {
        if (*it >= *end) continue;
        BlockReport b; b.begin = *it; b.end = *end;
        if (*end - *it <= 256) {
            std::vector<metail::Instruction> code(p.code.begin() + static_cast<std::ptrdiff_t>(*it), p.code.begin() + static_cast<std::ptrdiff_t>(*end));
            const auto tasks = metachain::isched::dependencies(code);
            b.critical_path = metachain::isched::critical_path(tasks);
            auto r = metachain::isched::list_schedule(tasks, machine, o.backend.scheduler == Scheduler::List);
            if (r) { b.makespan = r.makespan; b.scheduled = true; }
        }
        blocks.push_back(b);
    }
    return blocks;
}
// Schedule one straight-line MetaIL block with explicit machine options.
// Thin wrapper over isched so C++ callers need not reach past qscript.
inline metachain::isched::Result schedule_block(const std::vector<metail::Instruction>& code,
        const metachain::isched::Options& machine, Scheduler kind = Scheduler::List) {
    const auto tasks = metachain::isched::dependencies(code);
    switch (kind) {
    case Scheduler::Cycle: return metachain::isched::cycle_driven(tasks, machine);
    case Scheduler::Static: return metachain::isched::static_schedule(tasks, machine);
    default: return metachain::isched::list_schedule(tasks, machine);
    }
}
} // namespace qscript::backend
#endif
#endif
