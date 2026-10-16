#ifndef METACHAIN_QSCRIPT_DIAGNOSTICS_H
#define METACHAIN_QSCRIPT_DIAGNOSTICS_H
// Structured diagnostics and front-end introspection.
//
// After any failing context operation, qs_error() holds the formatted text
// and the same diagnostics are available one by one here, with their stage
// and source position. Strings belong to the context until its next
// operation.
#include "qscript/QScript.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum qs_stage {
    QS_STAGE_PARSE = 0, QS_STAGE_VERIFY = 1, QS_STAGE_OPTIMIZE = 2, QS_STAGE_EXECUTE = 3,
    QS_STAGE_ASSEMBLE = 4, QS_STAGE_LINK = 5, QS_STAGE_LOWER = 6, QS_STAGE_REWRITE = 7, QS_STAGE_IO = 8
} qs_stage;

typedef struct qs_diagnostic {
    int stage;                  /* qs_stage */
    const char *message;
    const char *file;           /* "" when unknown */
    size_t line, column;        /* 0 when unknown */
    size_t offset;              /* byte offset (parse) or instruction index (execute) */
} qs_diagnostic;

size_t qs_diagnostic_count(const qs_context *);
qs_status qs_diagnostic_get(const qs_context *, size_t index, qs_diagnostic *);
const char *qs_stage_name(int stage);

/* Parse and check without generating code. QS_OK means the source is valid. */
qs_status qs_check(qs_context *, const char *source, size_t length);
/* The QScript AST as an S-expression, e.g. (program (return (* (integer 6) (integer 7)) (@ 1:1:))). */
qs_status qs_ast_dump(qs_context *, const char *source, size_t length, const char **text, size_t *text_length);
/* The raw grammar match tree for source (see QScript-Grammar.h). */
qs_status qs_match_tree_dump(qs_context *, const char *source, size_t length, const char **text, size_t *text_length);

#ifdef __cplusplus
}
#endif
#endif
