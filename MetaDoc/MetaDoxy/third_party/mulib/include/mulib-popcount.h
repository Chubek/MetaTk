/**
 * @file mulib-popcount.h
 * @brief Bit population count, leading zeros, and trailing zeros for 64-bit words.
 * @defgroup mulib_popcount MuLIB popcount
 * @{
 * @details Portable fallback routines that avoid compiler-specific intrinsics. Zero is defined to have 64 leading/trailing zero bits.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_POPCOUNT before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_POPCOUNT_H
#define MULIB_POPCOUNT_H
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

/** @brief mulib popcount u64; see manual/mulib-popcount.md for contracts and examples. */
unsigned mulib_popcount_u64(uint64_t x);
/** @brief mulib popcount clz; see manual/mulib-popcount.md for contracts and examples. */
unsigned mulib_popcount_clz(uint64_t x);
/** @brief mulib popcount ctz; see manual/mulib-popcount.md for contracts and examples. */
unsigned mulib_popcount_ctz(uint64_t x);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_POPCOUNT_H */

#if defined(MULIB_IMPL_POPCOUNT) && !defined(MULIB_POPCOUNT_IMPLEMENTED)
#define MULIB_POPCOUNT_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
unsigned mulib_popcount_u64(uint64_t x){unsigned n=0;while(x){x&=x-1;n++;}return n;}
unsigned mulib_popcount_clz(uint64_t x){if(!x)return 64;unsigned n=0;for(uint64_t m=UINT64_C(1)<<63;!(x&m);m>>=1)n++;return n;}
unsigned mulib_popcount_ctz(uint64_t x){if(!x)return 64;unsigned n=0;while(!(x&1)){n++;x>>=1;}return n;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
