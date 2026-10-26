#ifndef GPPX_EMBED_API_H
#define GPPX_EMBED_API_H
/** \file GPPX-Embed.h
 * Binary-safe resource encoding for #embed plugin callbacks. Encoded strings
 * are malloc-owned; call gppx_free(). Encoding helpers accept length 0.
 */
#include "GPPX.h"
#ifdef __cplusplus
extern "C" {
#endif
GPPX_API int gppx_embed_hex(const unsigned char *bytes,size_t size,char **result);
GPPX_API int gppx_embed_c_array(const unsigned char *bytes,size_t size,char **result);
GPPX_API int gppx_embed_base64(const unsigned char *bytes,size_t size,char **result);
/** Adapter callbacks usable directly with gppx_register_embed(). */
GPPX_API int gppx_embed_hex_callback(GPPX_State *,const unsigned char *,size_t,char **,void *);
GPPX_API int gppx_embed_array_callback(GPPX_State *,const unsigned char *,size_t,char **,void *);
GPPX_API int gppx_embed_base64_callback(GPPX_State *,const unsigned char *,size_t,char **,void *);
static inline int gppx_embed_register(GPPX_State *state,const char *name,
                                      GPPX_EmbedFn fn,void *userdata) {
    return gppx_register_embed(state,name,fn,userdata);
}
#ifdef __cplusplus
}
#endif
#endif
