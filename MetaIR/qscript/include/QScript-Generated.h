#ifndef METACHAIN_QSCRIPT_GENERATED_H
#define METACHAIN_QSCRIPT_GENERATED_H
/* Runtime bridge for generated C99 programs that use grammars or matches.
   Link against MetaChain::QScript; this dispatches built-ins, not bytecode.
   A runtime owns its tables and heap. Its context must outlive it. */
#include "qscript/QScript.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct qs_runtime qs_runtime;
typedef struct qs_runtime_string { const char *data; size_t length; } qs_runtime_string;
typedef struct qs_runtime_grammar { const char *name, *source; size_t length; } qs_runtime_grammar;
typedef struct qs_runtime_config {
    const qs_runtime_string *strings;
    size_t string_count;
    const char *const *types;
    size_t type_count;
    const qs_runtime_grammar *grammars;
    size_t grammar_count;
    size_t max_heap_objects, max_heap_bytes;
} qs_runtime_config;
/* Output callbacks and heap limits are snapshotted at creation. Other context
   operations must not run concurrently with dispatch. Error text is in qs_error. */
qs_runtime *qs_runtime_create(qs_context *, const qs_runtime_config *);
void qs_runtime_destroy(qs_runtime *);
/* Registers must contain exactly 32 values, like a MetaIL Host instruction.
   Only QScript built-in ids are accepted; raw handles stay within this runtime. */
qs_status qs_runtime_dispatch(qs_context *, qs_runtime *, int64_t id,
                              const int64_t *registers, size_t count, int64_t *result);
#ifdef __cplusplus
}
#endif
#endif
