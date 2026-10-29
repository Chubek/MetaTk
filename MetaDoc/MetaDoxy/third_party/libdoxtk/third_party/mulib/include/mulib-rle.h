/**
 * @file mulib-rle.h
 * @brief Byte run-length encoding and bounded decoding.
 * @defgroup mulib_rle MuLIB rle
 * @{
 * @details Wire format repeats pairs `(count, byte)` where counts range 1..255. No checksum or length header is included; compressed data can be larger than source.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_RLE before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_RLE_H
#define MULIB_RLE_H
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

/** @brief mulib rle encode; see manual/mulib-rle.md for contracts and examples. */
size_t mulib_rle_encode(const unsigned char*in,size_t n,unsigned char*out,size_t cap);
/** @brief mulib rle decode; see manual/mulib-rle.md for contracts and examples. */
int mulib_rle_decode(const unsigned char*in,size_t n,unsigned char*out,size_t cap,size_t*written);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_RLE_H */

#if defined(MULIB_IMPL_RLE) && !defined(MULIB_RLE_IMPLEMENTED)
#define MULIB_RLE_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_rle_encode(const unsigned char*s,size_t n,unsigned char*out,size_t cap){if((!s&&n)||!out)return 0;size_t j=0;for(size_t i=0;i<n;){size_t run=1;while(i+run<n&&run<255&&s[i+run]==s[i])run++;if(j>cap||cap-j<2)return 0;out[j++]=(unsigned char)run;out[j++]=s[i];i+=run;}return j;}
int mulib_rle_decode(const unsigned char*s,size_t n,unsigned char*out,size_t cap,size_t*written){if((!s&&n)||(!out&&n)||n%2)return 0;size_t j=0;for(size_t i=0;i<n;i+=2){size_t run=s[i];if(!run||j>cap||run>cap-j)return 0;memset(out+j,s[i+1],run);j+=run;}if(written)*written=j;return 1;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
