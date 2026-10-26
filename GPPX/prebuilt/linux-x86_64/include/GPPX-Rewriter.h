#ifndef GPPX_REWRITER_API_H
#define GPPX_REWRITER_API_H
/** \file GPPX-Rewriter.h
 * Rewriters run on each expanded, non-directive output line, in registration
 * order. They should be deterministic and return malloc-owned replacements.
 */
#include "GPPX.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Replace all nonoverlapping occurrences; empty needle is an error.
 * Preserves binary-unaware C-string semantics (not suitable for raw files).
 */
GPPX_API int gppx_rewriter_replace_literal(const char *input,const char *needle,
                                            const char *replacement,char **result);
typedef struct GPPX_LiteralRewriter {const char *needle;const char *replacement;} GPPX_LiteralRewriter;
/** Stock rewriter callback. userdata must point to a live configuration. */
GPPX_API int gppx_rewriter_literal_callback(GPPX_State *,const char *input,
                                              char **result,void *userdata);
static inline int gppx_rewriter_register(GPPX_State *state,GPPX_RewriteFn fn,void *userdata) {
    return gppx_register_rewriter(state,fn,userdata);
}
#ifdef __cplusplus
}
#endif
#endif
