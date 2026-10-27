#ifndef GPPX_JACKSCRIPT_API_H
#define GPPX_JACKSCRIPT_API_H
#include "GPPX.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Select the jackscript profile (# directives, explicit $(MACRO) expansion)
 * and register a UTF-8 reader for .jcks files. A leading BOM is removed and
 * a first-line shebang is preserved. This is preprocessing, not execution. */
GPPX_API int gppx_jackscript_enable(GPPX_State*);
GPPX_API int gppx_jackscript_reader(GPPX_State*,const char *path,
                                   char **output,void *userdata);
#ifdef __cplusplus
}
#endif
#endif
