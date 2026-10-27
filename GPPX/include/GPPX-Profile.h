#ifndef GPPX_PROFILE_API_H
#define GPPX_PROFILE_API_H
#include "GPPX-Style.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Apply a base profile followed by optional overrides. NULL fields preserve
 * current values; style may be NULL. Errors do not roll back earlier options. */
typedef struct GPPX_Profile {
    const char *base; /* c, percent, template, jackscript */
    const char *directive;
    const GPPX_Style *style;
} GPPX_Profile;
GPPX_API int gppx_profile_apply(GPPX_State*,const GPPX_Profile*);
GPPX_API size_t gppx_profile_count(void);
/* Static string, or NULL for an out-of-range index. */
GPPX_API const char *gppx_profile_name(size_t index);
#ifdef __cplusplus
}
#endif
#endif
