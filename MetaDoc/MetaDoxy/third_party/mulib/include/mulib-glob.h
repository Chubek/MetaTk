/**
 * @file mulib-glob.h
 * @brief Portable glob pattern matching with star and question-mark wildcards.
 * @defgroup mulib_glob MuLIB glob
 * @{
 * @details Matches the entire input string with `?` for exactly one byte and `*` for zero or more bytes. Matching is byte-oriented and not POSIX filename globbing; no character classes or path separator rules.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_GLOB before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_GLOB_H
#define MULIB_GLOB_H
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

/** @brief mulib glob match; see manual/mulib-glob.md for contracts and examples. */
int mulib_glob_match(const char*pattern,const char*text);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_GLOB_H */

#if defined(MULIB_IMPL_GLOB) && !defined(MULIB_GLOB_IMPLEMENTED)
#define MULIB_GLOB_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_glob_match(const char*p,const char*s){if(!p||!s)return 0;const char*star=NULL,*retry=NULL;while(*s){if(*p=='?'||*p==*s){p++;s++;}else if(*p=='*'){star=p++;retry=s;}else if(star){p=star+1;s=++retry;}else return 0;}while(*p=='*')p++;return !*p;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
