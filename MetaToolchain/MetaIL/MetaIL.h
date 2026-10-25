/* MetaIL C11 API backed by the real Lua 5.4 runtime.
 * Each context owns an independent runtime. Different contexts may be used in
 * parallel; calls on the same context must be serialized by the caller.
 * All source/name/pass arguments are NUL-terminated UTF-8 strings. */
#ifndef METAIL_H
#define METAIL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct metail_context metail_context;
typedef enum metail_status {
    METAIL_OK = 0,
    METAIL_INVALID_ARGUMENT = 1,
    METAIL_RUNTIME_ERROR = 2,
    METAIL_OUT_OF_MEMORY = 3
} metail_status;

/* Metadata strings are borrowed until the next call on this context.
 * keywords, tags, and uses are newline-separated lists (possibly empty).
 * path names the actual loaded .optk file; kind is analysis/rewrite/builtin. */
typedef struct metail_kernel_info {
    const char *name, *path, *kind, *implementation, *description;
    const char *keywords, *tags, *uses;
} metail_kernel_info;

/* home is a runtime root containing lua/ and optimize-kernels/. NULL uses
 * METAIL_HOME, the current directory, then configured install/source roots.
 * Returns NULL on failure and writes a truncated, NUL-terminated diagnostic
 * when error != NULL and error_size > 0. No process-global error storage. */
metail_context *metail_create(const char *home, char *error, size_t error_size);
void metail_destroy(metail_context *context); /* NULL is accepted. */
/* Borrowed diagnostic, valid until the next call; NULL context is accepted. */
const char *metail_last_error(const metail_context *context);

/* Output strings are allocated by the library; release with metail_free().
 * Output slots must be distinct and are set to NULL on failure. name may be
 * NULL (<memory>). Errors leave the context reusable. No input is modified. */
void metail_free(void *memory);
metail_status metail_check(metail_context *, const char *source, const char *name);
metail_status metail_emit(metail_context *, const char *source, const char *name, char **output);
/* passes is NULL/"all" or a nonempty comma-separated list of exact kernel names.
 * rounds must be 1..12. stats may be NULL, otherwise receives JSON change counts.
 * The default all-pass pipeline includes final cleanup on every round. */
metail_status metail_optimize(metail_context *, const char *source, const char *name,
                             const char *passes, int rounds, char **output, char **stats);
metail_status metail_analyze(metail_context *, const char *source, const char *name,
                            const char *passes, char **json);

/* Structured IR: descriptors mirror lua/metail/ir.lua. Input descriptors and
 * their strings/arrays are borrowed only for the duration of a call. Initialize
 * unused fields to zero. Shared expression subtrees are allowed; cycles are not.
 * Identifiers use [A-Za-z_][A-Za-z0-9_]*. Operators use canonical names such as
 * add, mul, neg, select; metail_operator_arity returns -1 for unknown names. */
typedef enum metail_expr_kind { METAIL_INT, METAIL_VAR, METAIL_OP, METAIL_CALL } metail_expr_kind;
typedef struct metail_expr {
    metail_expr_kind kind;
    int64_t value;                      /* INT */
    const char *name;                   /* VAR or CALL */
    const char *op;                     /* OP */
    const struct metail_expr *const *args; /* OP or CALL; ordered */
    size_t arg_count;
} metail_expr;
typedef enum metail_stmt_kind {
    METAIL_LET, METAIL_RETURN, METAIL_EVAL, METAIL_ASSUME,
    METAIL_LABEL, METAIL_GOTO, METAIL_BRANCH
} metail_stmt_kind;
typedef struct metail_stmt {
    metail_stmt_kind kind;
    const char *name;                  /* LET or LABEL */
    const metail_expr *expr;           /* LET, RETURN, EVAL, ASSUME, BRANCH */
    const char *target;                /* GOTO */
    const char *yes, *no;              /* BRANCH */
} metail_stmt;
typedef struct metail_function {
    const char *name;
    const char *const *params;
    size_t param_count;
    const metail_stmt *statements;
    size_t statement_count;
} metail_function;
typedef struct metail_module {
    const char *name;                  /* Optional module name; NULL omits it. */
    const metail_function *functions;
    size_t function_count;
} metail_module;

int metail_operator_arity(const char *op);
/* Parse text once into an independently owned, inspectable structured module. */
metail_status metail_parse(metail_context *, const char *source, const char *name, metail_module **output);
/* Free ONLY modules returned by metail_parse/metail_module_optimize, never
 * caller-built descriptors. NULL is accepted. Returned modules and all nested
 * pointers remain valid after context destruction; treat them as read-only. */
void metail_module_free(metail_module *module);
/* Direct structured operations: no serialization or textual parser in this path.
 * Bounds: depth <= 256, <= 200000 structural items, <= 8 MiB string bytes per
 * import/export. Invalid descriptors return an error; inputs are never mutated.
 * Validation includes structure, arity, identifiers, duplicate names and CFG
 * targets, but does not prove variables initialized or calls resolvable. */
metail_status metail_module_check(metail_context *, const metail_module *);
metail_status metail_module_emit(metail_context *, const metail_module *, char **output);
metail_status metail_module_analyze(metail_context *, const metail_module *, const char *passes, char **json);
metail_status metail_module_optimize(metail_context *, const metail_module *, const char *passes,
                                    int rounds, metail_module **output, char **stats);
/* Built-in kernels load lazily on first use. Additional directories are loaded
 * recursively and transactionally; duplicates and invalid kernels are errors. */
metail_status metail_load_kernels(metail_context *, const char *directory);
metail_status metail_kernel_count(metail_context *, size_t *count);
metail_status metail_kernel_get(metail_context *, size_t index, metail_kernel_info *info);
metail_status metail_kernels_json(metail_context *, char **json);

/* CLI entry point, using the same parser, passes and runtime as the library.
 * argc >= 1 and argv[0..argc-1] must be valid strings. Returns a process code. */
int metail_cli_main(int argc, char **argv);
#ifdef __cplusplus
}
#endif
#endif
