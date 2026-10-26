#ifndef GPPX_MACRO_API_H
#define GPPX_MACRO_API_H
/** \file GPPX-Macro.h
 * Macros created via #define / #macro or with gppx_define. Dynamic macro
 * callbacks are invoked with already-expanded positional arguments.
 * Object macro definitions persist across preprocessing calls on the state.
 */
#include "GPPX.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Define from CLI notation NAME or NAME=VALUE; NAME defaults to "1". */
GPPX_API int gppx_macro_define_spec(GPPX_State *state, const char *spec);
/** Run the regular preprocessor over an expression as a standalone source.
 * This evaluates directives too; it is NOT a standalone macro-only expansion.
 * Caller owns *output, which must be freed with gppx_free().
 */
GPPX_API int gppx_macro_process_text(GPPX_State *state, const char *text, char **output);
static inline int gppx_macro_register(GPPX_State *state, const char *name,
                                      GPPX_MacroFn fn, void *userdata) {
    return gppx_register_macro(state,name,fn,userdata);
}
#ifdef __cplusplus
}
#endif
#endif
