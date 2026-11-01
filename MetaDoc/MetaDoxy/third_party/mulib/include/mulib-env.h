/**
 * @file mulib-env.h
 * @brief Environment variable inspection and bounded fallback copy.
 * @defgroup mulib_env MuLIB env
 * @{
 * @details Accesses the process environment through the C runtime. Concurrent mutation of environment variables must be synchronized by the application.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ENV before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_ENV_H
#define MULIB_ENV_H
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

/** @brief mulib env present; see manual/mulib-env.md for contracts and examples. */
int mulib_env_present(const char*name);
/** @brief mulib env copy; see manual/mulib-env.md for contracts and examples. */
int mulib_env_copy(const char*name,const char*fallback,char*out,size_t capacity);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ENV_H */

#if defined(MULIB_IMPL_ENV) && !defined(MULIB_ENV_IMPLEMENTED)
#define MULIB_ENV_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_env_present(const char*n){return n&&getenv(n)!=NULL;}
int mulib_env_copy(const char*n,const char*fallback,char*out,size_t cap){if(!n||!out)return 0;const char*s=getenv(n);if(!s)s=fallback?fallback:"";size_t len=strlen(s);if(len>=cap)return 0;memcpy(out,s,len+1);return 1;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
