/**
 * @file mulib-ini.h
 * @brief Callback-based INI section and key-value line parser.
 * @defgroup mulib_ini MuLIB ini
 * @{
 * @details Accepts [section] headings and key=value pairs, ignores comments beginning with # or ; at the start of a trimmed line, and stops when callback returns zero. Quoted multiline values and escape semantics are outside this format subset.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_INI before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_INI_H
#define MULIB_INI_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <inttypes.h>
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Callback-based INI section and key-value line parser. */
typedef int (*mulib_ini_callback)(void*user,const char*section,const char*key,const char*value);
/** @brief mulib ini parse; see manual/mulib-ini.md for contracts and examples. */
int mulib_ini_parse(const char*source,mulib_ini_callback callback,void*user);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_INI_H */

#if defined(MULIB_IMPL_INI) && !defined(MULIB_INI_IMPLEMENTED)
#define MULIB_INI_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static char*mulib_ini_trim_(char*s){while(isspace((unsigned char)*s))s++;size_t n=strlen(s);while(n&&isspace((unsigned char)s[n-1]))s[--n]=0;return s;}
int mulib_ini_parse(const char*src,mulib_ini_callback cb,void*user){if(!src||!cb)return 0;size_t n=strlen(src);char*buf=malloc(n+1);if(!buf)return 0;memcpy(buf,src,n+1);char *section=calloc(n+1,1);if(!section){free(buf);return 0;}int ok=1;char*cursor=buf;while(*cursor){char*line=cursor;while(*cursor&&*cursor!='\n')cursor++;if(*cursor)*cursor++=0;char*s=mulib_ini_trim_(line);if(!*s||*s==';'||*s=='#')continue;if(*s=='['){char*e=strchr(s+1,']');if(!e||*mulib_ini_trim_(e+1)){ok=0;break;}*e=0;strcpy(section,mulib_ini_trim_(s+1));}else{char*eq=strchr(s,'=');if(!eq){ok=0;break;}*eq=0;if(!cb(user,section,mulib_ini_trim_(s),mulib_ini_trim_(eq+1))){ok=0;break;}}}free(section);free(buf);return ok;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
