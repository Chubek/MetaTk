/**
 * @file mulib-path.h
 * @brief Portable lexical path manipulations for slash-separated paths.
 * @defgroup mulib_path MuLIB path
 * @{
 * @details Performs string-level operations only: does not resolve symlinks, canonicalize dot segments, or check the existence of a path.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_PATH before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_PATH_H
#define MULIB_PATH_H
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

/** @brief mulib path basename; see manual/mulib-path.md for contracts and examples. */
const char *mulib_path_basename(const char*path);
/** @brief mulib path extension; see manual/mulib-path.md for contracts and examples. */
const char *mulib_path_extension(const char*path);
/** @brief mulib path join; see manual/mulib-path.md for contracts and examples. */
int mulib_path_join(const char*directory,const char*name,char*out,size_t cap);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_PATH_H */

#if defined(MULIB_IMPL_PATH) && !defined(MULIB_PATH_IMPLEMENTED)
#define MULIB_PATH_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
const char*mulib_path_basename(const char*p){if(!p)return NULL;const char*a=strrchr(p,'/'),*b=strrchr(p,'\\');if(b&&(!a||b>a))a=b;return a?a+1:p;}
const char*mulib_path_extension(const char*p){const char*base=mulib_path_basename(p);if(!base)return NULL;const char*dot=strrchr(base,'.');return dot&&dot!=base?dot+1:base+strlen(base);}
int mulib_path_join(const char*d,const char*n,char*out,size_t cap){if(!d||!n||!out)return 0;size_t a=strlen(d),b=strlen(n),sep=a&&d[a-1]!='/'&&d[a-1]!='\\';if(a>SIZE_MAX-b-1-sep||a+b+sep+1>cap)return 0;memcpy(out,d,a);if(sep)out[a++]='/';memcpy(out+a,n,b+1);return 1;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
