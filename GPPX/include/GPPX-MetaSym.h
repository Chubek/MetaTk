#ifndef GPPX_METASYM_API_H
#define GPPX_METASYM_API_H
#include "GPPX.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Construct a macro identifier: namespace + "_" + name, or just name
 * for a NULL/empty namespace. Both components must be ASCII identifiers.
 * This is textual qualification: (a_b,c) and (a,b_c) intentionally alias.
 * Output is malloc-owned. No separate symbol table or engine state is kept. */
GPPX_API int gppx_metasym_name(const char *name_space,const char *name,char **output);
GPPX_API int gppx_metasym_define(GPPX_State *state,const char *name_space,const char *name,const char *replacement);
GPPX_API int gppx_metasym_undef(GPPX_State *state,const char *name_space,const char *name);
#ifdef __cplusplus
}
#endif
#endif
