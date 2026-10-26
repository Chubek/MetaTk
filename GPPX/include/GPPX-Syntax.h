#ifndef GPPX_SYNTAX_API_H
#define GPPX_SYNTAX_API_H
/** \file GPPX-Syntax.h
 * Retargetable syntax: `c`, `percent`, `template` built-in profiles.
 * The directive marker may be multiple bytes, as may the macro delimiters.
 * Profiles reset delimiters; apply profile before individual overrides.
 */
#include "GPPX.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Apply a built-in syntax profile. Nonzero means invalid profile/state. */
GPPX_API int gppx_syntax_apply_profile(GPPX_State *state,const char *profile);
/** Set directive marker and macro delimiters; NULL means leave unchanged.
 * Configurations are applied sequentially, not transactionally.
 */
GPPX_API int gppx_syntax_delimiters(GPPX_State *state,const char *directive,
                                     const char *macro_open,const char *macro_close);
static inline int gppx_syntax_register(GPPX_State *state,const char *option,
                                       GPPX_SyntaxFn fn,void *userdata) {
    return gppx_register_syntax(state,option,fn,userdata);
}
#ifdef __cplusplus
}
#endif
#endif
