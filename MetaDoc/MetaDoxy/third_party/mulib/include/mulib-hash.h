/**
 * @file mulib-hash.h
 * @brief Fast non-cryptographic FNV-1a hashing for raw buffers and text.
 * @defgroup mulib_hash MuLIB hash
 * @{
 * @details FNV-1a is useful for hash tables and checksums where adversaries do not control inputs. It provides no collision resistance.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_HASH before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_HASH_H
#define MULIB_HASH_H
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

/** @brief mulib hash fnv1a; see manual/mulib-hash.md for contracts and examples. */
uint64_t mulib_hash_fnv1a(const void*data,size_t length);
/** @brief mulib hash string; see manual/mulib-hash.md for contracts and examples. */
uint64_t mulib_hash_string(const char*text);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_HASH_H */

#if defined(MULIB_IMPL_HASH) && !defined(MULIB_HASH_IMPLEMENTED)
#define MULIB_HASH_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
uint64_t mulib_hash_fnv1a(const void*data,size_t n){if(!data&&n)return 0;const unsigned char*p=data;uint64_t h=UINT64_C(14695981039346656037);for(size_t i=0;i<n;i++){h^=p[i];h*=UINT64_C(1099511628211);}return h;}
uint64_t mulib_hash_string(const char*s){return s?mulib_hash_fnv1a(s,strlen(s)):0;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
