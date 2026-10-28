/**
 * @file mulib-clamp.h
 * @brief Saturating arithmetic and interval clamping for signed integers.
 * @defgroup mulib_clamp MuLIB clamp
 * @{
 * @details Addition/subtraction saturate at INT64_MIN or INT64_MAX on overflow. Clamp expects low<=high.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_CLAMP before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_CLAMP_H
#define MULIB_CLAMP_H
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

/** @brief mulib clamp i64; see manual/mulib-clamp.md for contracts and examples. */
int64_t mulib_clamp_i64(int64_t value,int64_t low,int64_t high);
/** @brief mulib clamp add i64; see manual/mulib-clamp.md for contracts and examples. */
int64_t mulib_clamp_add_i64(int64_t a,int64_t b);
/** @brief mulib clamp sub i64; see manual/mulib-clamp.md for contracts and examples. */
int64_t mulib_clamp_sub_i64(int64_t a,int64_t b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_CLAMP_H */

#if defined(MULIB_IMPL_CLAMP) && !defined(MULIB_CLAMP_IMPLEMENTED)
#define MULIB_CLAMP_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int64_t mulib_clamp_i64(int64_t x,int64_t lo,int64_t hi){return x<lo?lo:x>hi?hi:x;}
int64_t mulib_clamp_add_i64(int64_t a,int64_t b){if(b>0&&a>INT64_MAX-b)return INT64_MAX;if(b<0&&a<INT64_MIN-b)return INT64_MIN;return a+b;}
int64_t mulib_clamp_sub_i64(int64_t a,int64_t b){if(b<0&&a>INT64_MAX+b)return INT64_MAX;if(b>0&&a<INT64_MIN+b)return INT64_MIN;return a-b;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
