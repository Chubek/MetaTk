#ifndef METAVM_COMPILER_H
#define METAVM_COMPILER_H
#include "Module.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Compile bounded assembly text (not NUL-terminated). Embedded NUL is invalid.
 * Output is an owned, verified module; NULL on failure. See TerseSyntax.h.
 * max_bytes==0 selects the default. Diagnostics include 1-based source lines. */
mvm_status mvm_compile(const char *source, size_t length, size_t max_bytes,
    mvm_module **output, mvm_error *);
#ifdef __cplusplus
}
#endif
#endif
