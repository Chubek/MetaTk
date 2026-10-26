#ifndef GPPX_DIRECTIVE_API_H
#define GPPX_DIRECTIVE_API_H
/** \file GPPX-Directive.h
 * Directive authoring helpers. A directive callback is passed the portion of
 * the directive following its name. The returned string is owned by GPPX.
 * Functions below return 0 on success and 1 on failure, unless stated.
 */
#include "GPPX.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Return 1 only for an ASCII identifier beginning with a letter or '_'. */
GPPX_API int gppx_directive_valid_name(const char *name);
/** Split `identifier whitespace arguments` into two malloc-owned strings.
 * At entry, both output locations must be valid; on failure they remain NULL.
 * Caller frees each using gppx_free(). Empty arguments are allowed.
 */
GPPX_API int gppx_directive_split(const char *text, char **name, char **arguments);
/** Register a directive. Callback replacement (if any) must use malloc(). */
static inline int gppx_directive_register(GPPX_State *state, const char *name,
                                           GPPX_DirectiveFn fn, void *userdata) {
    return gppx_directive_valid_name(name) ? gppx_register_directive(state,name,fn,userdata) : 1;
}
#ifdef __cplusplus
}
#endif
#endif
