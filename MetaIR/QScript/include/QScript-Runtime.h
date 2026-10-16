#ifndef METACHAIN_QSCRIPT_RUNTIME_H
#define METACHAIN_QSCRIPT_RUNTIME_H
// QScript runtime: execution engines, resource limits, run statistics,
// program introspection and calling QScript functions from the host.
//
//   qs_program *p = qs_compile(c, "use math; fn hyp(a, b) { return isqrt(a*a + b*b); }", n);
//   int64_t args[] = {3, 4}, r;
//   qs_call(c, p, "hyp", args, 2, &r);        // r == 5
//
// Every function the program itself defines is callable, plus any library
// function the program uses. Calls run on the same VM with the same limits.
#include "qscript/QScript.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum qs_engine {
    QS_ENGINE_INTERPRETER = 0,  /* portable MetaVM interpreter (default) */
    QS_ENGINE_JIT = 1           /* MetaJIT; falls back to the interpreter off x86-64 Linux */
} qs_engine;

typedef enum qs_run_status {
    QS_RUN_HALTED = 0,          /* return/exit/end of program */
    QS_RUN_TRAPPED = 1,         /* division by zero, failed assert, stack overflow, ... */
    QS_RUN_STEP_LIMIT = 2       /* qs_limits.max_steps exhausted */
} qs_run_status;

/* In qs_set_limits a zero field means "keep the current value". */
typedef struct qs_limits {
    size_t max_steps;           /* VM instructions per run */
    size_t max_stack;           /* VM operand-stack values (bounds recursion) */
    size_t max_instructions;    /* MetaIL program size */
    size_t max_depth;           /* syntax/AST nesting (capped at 256) */
    size_t max_nodes;           /* AST nodes per source */
    size_t max_source_bytes;    /* bytes per source */
} qs_limits;

typedef struct qs_run_info {
    int64_t result;
    size_t steps;
    size_t outputs;             /* values printed (also sent to qs_set_output) */
    int status;                 /* qs_run_status */
} qs_run_info;

qs_status qs_get_limits(const qs_context *, qs_limits *);
qs_status qs_set_limits(qs_context *, const qs_limits *);
qs_status qs_set_engine(qs_context *, int engine);
int qs_get_engine(const qs_context *);

/* Like qs_run, but reports status and statistics. Returns QS_OK only when
   the program halted normally; info is filled in either way. */
qs_status qs_run_ex(qs_context *, const qs_program *, qs_run_info *);
/* Call a function by name ("f", or "lib::f" for a library function). */
qs_status qs_call(qs_context *, const qs_program *, const char *function,
                  const int64_t *args, size_t count, int64_t *result);

size_t qs_program_size(const qs_program *);                       /* MetaIL instructions */
size_t qs_program_function_count(const qs_program *);
const char *qs_program_function_name(const qs_program *, size_t index);
size_t qs_program_function_arity(const qs_program *, size_t index);
size_t qs_program_library_count(const qs_program *);
const char *qs_program_library_name(const qs_program *, size_t index);

#ifdef __cplusplus
}
#endif
#endif
