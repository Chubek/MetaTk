#ifndef GPPX_STYLE_API_H
#define GPPX_STYLE_API_H
#include "GPPX.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Syntax presentation options. NULL strings leave an option unchanged.
 * expand_strings is -1 (unchanged), 0 (off), or 1 (on).
 * Options apply sequentially; engine errors do not roll earlier options back.
 * This helper configures existing syntax slots, not a new plugin ABI slot. */
typedef struct GPPX_Style {
    const char *macro_style; /* bare, dollar, or mustache */
    const char *line_comment;
    const char *block_open;
    const char *block_close;
    int expand_strings;
} GPPX_Style;
GPPX_API int gppx_style_apply(GPPX_State *state,const GPPX_Style *style);
#ifdef __cplusplus
}
#endif
#endif
