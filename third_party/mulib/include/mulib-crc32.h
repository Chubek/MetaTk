/**
 * @file mulib-crc32.h
 * @brief IEEE CRC-32 checksum with incremental updates.
 * @defgroup mulib_crc32 MuLIB crc32
 * @{
 * @details Use seed zero for a fresh CRC. Passing a completed CRC as seed continues the same checksum; result is little-endian reflected CRC-32/ISO-HDLC.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_CRC32 before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_CRC32_H
#define MULIB_CRC32_H
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

/** @brief mulib crc32 update; see manual/mulib-crc32.md for contracts and examples. */
uint32_t mulib_crc32_update(uint32_t seed,const void*data,size_t n);
/** @brief mulib crc32; see manual/mulib-crc32.md for contracts and examples. */
uint32_t mulib_crc32(const void*data,size_t n);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_CRC32_H */

#if defined(MULIB_IMPL_CRC32) && !defined(MULIB_CRC32_IMPLEMENTED)
#define MULIB_CRC32_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
uint32_t mulib_crc32_update(uint32_t seed,const void*p,size_t n){if(!p&&n)return seed;uint32_t c=~seed;const unsigned char*b=p;for(size_t i=0;i<n;i++){c^=b[i];for(int j=0;j<8;j++)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}return ~c;}
uint32_t mulib_crc32(const void*p,size_t n){return mulib_crc32_update(0,p,n);}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
