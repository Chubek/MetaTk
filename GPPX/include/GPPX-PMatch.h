#ifndef GPPX_PMATCH_API_H
#define GPPX_PMATCH_API_H
/** \file GPPX-PMatch.h
 * Pluggable matchers for #ifmatch/#elifmatch. Positive = match, zero = miss;
 * negative = callback error. Matchers receive pattern, then subject.
 */
#include "GPPX.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Entire-string glob matching: ? matches one byte, * matches any sequence;
 * backslash escapes * ? and backslash. ASCII byte-oriented (not UTF-8-aware).
 */
GPPX_API int gppx_pmatch_glob(const char *pattern,const char *subject);
/** Entire-string matcher for a literal prefix (including empty prefix). */
GPPX_API int gppx_pmatch_prefix(const char *pattern,const char *subject);
GPPX_API int gppx_pmatch_glob_callback(GPPX_State *,const char *,const char *,void *);
GPPX_API int gppx_pmatch_prefix_callback(GPPX_State *,const char *,const char *,void *);
static inline int gppx_pmatch_register(GPPX_State *state,const char *name,
                                       GPPX_PMatchFn fn,void *userdata) {
    return gppx_register_pmatch(state,name,fn,userdata);
}
#ifdef __cplusplus
}
#endif
#endif
