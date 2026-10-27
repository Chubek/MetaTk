#ifndef GPPX_UTF8INPUT_API_H
#define GPPX_UTF8INPUT_API_H
#include "GPPX.h"
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
/** Decode one Unicode scalar at *offset and advance it only on success.
 * Rejects overlong encodings, surrogates, truncation and values > U+10FFFF.
 * End of input is an error. A zero scalar is valid for this byte API. */
GPPX_API int gppx_utf8_next(const unsigned char *bytes,size_t size,size_t *offset,uint32_t *scalar);
/** Validate bytes; NULL is allowed only for size=0. error_offset receives
 * size on success, otherwise the start of the first invalid sequence. */
GPPX_API int gppx_utf8_validate(const unsigned char *bytes,size_t size,size_t *error_offset);
/** Load validated UTF-8 text, stripping a leading BOM. Reject embedded NUL
 * to prevent truncation at the engine's string interface. malloc-owned output. */
GPPX_API int gppx_utf8_load(const char *path,char **output);
GPPX_API int gppx_utf8_reader_callback(GPPX_State *state,const char *path,char **output,void *userdata);
#ifdef __cplusplus
}
#endif
#endif
