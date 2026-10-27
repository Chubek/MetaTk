#ifndef GPPX_FUNCTION_API_H
#define GPPX_FUNCTION_API_H
#include "GPPX.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Define a function macro. parameters is a comma-separated identifier list;
 * a final ... declares __VA_ARGS__. The engine copies all strings. */
GPPX_API int gppx_function_define(GPPX_State*,const char *name,
                                const char *parameters,const char *body);
/* Invoke a macro or native callable directly, independent of syntax profile.
 * Arguments are preprocessing text. Output is allocated with malloc.
 * Zero arguments permits argv == NULL. */
GPPX_API int gppx_function_call(GPPX_State*,const char *name,int argc,
                              const char *const *argv,char **output);
#ifdef __cplusplus
}
#endif
#endif
