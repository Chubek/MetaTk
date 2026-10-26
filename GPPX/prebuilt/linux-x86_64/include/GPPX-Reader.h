#ifndef GPPX_READER_API_H
#define GPPX_READER_API_H
/** \file GPPX-Reader.h
 * Reader callbacks are selected by extension (including leading dot); they
 * return NUL-terminated TEXT for the preprocessing pipeline. Raw binary
 * resources are supported by the byte-oriented helper below and #embed.
 */
#include "GPPX.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Read whole file, with independent byte length. Appends an extra NUL byte,
 * not included in *size. Result must be freed with gppx_free(). Never try to
 * treat embedded NUL bytes as input to gppx_process().
 */
GPPX_API int gppx_reader_load_bytes(const char *path,unsigned char **data,size_t *size);
static inline int gppx_reader_register(GPPX_State *state,const char *extension,
                                       GPPX_ReaderFn fn,void *userdata) {
    return gppx_register_reader(state,extension,fn,userdata);
}
#ifdef __cplusplus
}
#endif
#endif
