/**
 * @file mulib-endian.h
 * @brief Explicit little- and big-endian primitive load/store helpers.
 * @defgroup mulib_endian MuLIB endian
 * @{
 * @details Safe on unaligned byte buffers; unlike pointer casts these functions avoid alignment and strict-aliasing violations.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ENDIAN before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_ENDIAN_H
#define MULIB_ENDIAN_H
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

/** @brief mulib endian load le32; see manual/mulib-endian.md for contracts and examples. */
uint32_t mulib_endian_load_le32(const unsigned char *bytes);
/** @brief mulib endian load be32; see manual/mulib-endian.md for contracts and examples. */
uint32_t mulib_endian_load_be32(const unsigned char *bytes);
/** @brief mulib endian store le32; see manual/mulib-endian.md for contracts and examples. */
void mulib_endian_store_le32(unsigned char *bytes,uint32_t value);
/** @brief mulib endian store be32; see manual/mulib-endian.md for contracts and examples. */
void mulib_endian_store_be32(unsigned char *bytes,uint32_t value);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ENDIAN_H */

#if defined(MULIB_IMPL_ENDIAN) && !defined(MULIB_ENDIAN_IMPLEMENTED)
#define MULIB_ENDIAN_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
uint32_t mulib_endian_load_le32(const unsigned char*b){return b?(uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24:0;}
uint32_t mulib_endian_load_be32(const unsigned char*b){return b?(uint32_t)b[3]|(uint32_t)b[2]<<8|(uint32_t)b[1]<<16|(uint32_t)b[0]<<24:0;}
void mulib_endian_store_le32(unsigned char*b,uint32_t v){if(b)for(unsigned i=0;i<4;i++)b[i]=(unsigned char)(v>>(i*8));}
void mulib_endian_store_be32(unsigned char*b,uint32_t v){if(b)for(unsigned i=0;i<4;i++)b[i]=(unsigned char)(v>>((3-i)*8));}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
