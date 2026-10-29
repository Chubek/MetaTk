/**
 * @file mulib-bitset.h
 * @brief Resizable bit-array with test, set, clear, and population count.
 * @defgroup mulib_bitset MuLIB bitset
 * @{
 * @details Bit indices start at zero. Unused high bits in the last byte stay zero because allocation is zero-filled.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_BITSET before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_BITSET_H
#define MULIB_BITSET_H
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

/** @brief Resizable bit-array with test, set, clear, and population count. */
typedef struct mulib_bitset { unsigned char *bytes; size_t bits; } mulib_bitset_t;
/** @brief mulib bitset init; see manual/mulib-bitset.md for contracts and examples. */
int mulib_bitset_init(mulib_bitset_t*b,size_t bits);
/** @brief mulib bitset test; see manual/mulib-bitset.md for contracts and examples. */
int mulib_bitset_test(const mulib_bitset_t*b,size_t i);
/** @brief mulib bitset set; see manual/mulib-bitset.md for contracts and examples. */
int mulib_bitset_set(mulib_bitset_t*b,size_t i,int value);
/** @brief mulib bitset count; see manual/mulib-bitset.md for contracts and examples. */
size_t mulib_bitset_count(const mulib_bitset_t*b);
/** @brief mulib bitset free; see manual/mulib-bitset.md for contracts and examples. */
void mulib_bitset_free(mulib_bitset_t*b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_BITSET_H */

#if defined(MULIB_IMPL_BITSET) && !defined(MULIB_BITSET_IMPLEMENTED)
#define MULIB_BITSET_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_bitset_init(mulib_bitset_t*b,size_t n){if(!b||n>SIZE_MAX-7)return 0;b->bits=n;b->bytes=calloc((n+7)/8? (n+7)/8:1,1);return b->bytes!=NULL;}
int mulib_bitset_test(const mulib_bitset_t*b,size_t i){return b&&i<b->bits?(b->bytes[i/8]>>(i%8))&1:0;}
int mulib_bitset_set(mulib_bitset_t*b,size_t i,int v){if(!b||i>=b->bits)return 0;unsigned char mask=(unsigned char)(1u<<(i%8));if(v)b->bytes[i/8]|=mask;else b->bytes[i/8]&=(unsigned char)~mask;return 1;}
size_t mulib_bitset_count(const mulib_bitset_t*b){if(!b||!b->bytes)return 0;size_t cnt=0;for(size_t i=0;i<(b->bits+7)/8;i++){unsigned char x=b->bytes[i];while(x){cnt++;x&=(unsigned char)(x-1);}}return cnt;}
void mulib_bitset_free(mulib_bitset_t*b){if(b){free(b->bytes);b->bytes=NULL;b->bits=0;}}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
