#ifndef GPPX_LAYOUT_API_H
#define GPPX_LAYOUT_API_H
#include "GPPX.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Normalize CRLF and CR to LF and optionally expand tabs to tab stops.
 * tab_width=0 preserves tabs. Columns count bytes (not display cells).
 * Indent prefixes every nonempty line with indent_spaces spaces. Existing
 * whitespace and the presence/absence of a final newline are preserved.
 * Output is malloc-owned; release with gppx_free(). */
typedef struct GPPX_Layout {
    size_t tab_width;
    size_t indent_spaces;
} GPPX_Layout;
GPPX_API int gppx_layout_format(const char *text,const GPPX_Layout *layout,char **output);
/** userdata points to a live GPPX_Layout for the registration lifetime.
 * The engine invokes rewriters once per line. */
GPPX_API int gppx_layout_callback(GPPX_State *state,const char *text,char **output,void *userdata);
#ifdef __cplusplus
}
#endif
#endif
