/**
 * @file mulib-ring.h
 * @brief Fixed-capacity circular buffer which refuses to overwrite unread items.
 * @defgroup mulib_ring MuLIB ring
 * @{
 * @details Capacity is fixed at initialization, and writes fail without mutation when full.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_RING before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_RING_H
#define MULIB_RING_H
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

/** @brief Fixed-capacity circular buffer which refuses to overwrite unread items. */
typedef struct mulib_ring { unsigned char *data; size_t head,length,capacity,stride; } mulib_ring_t;
/** @brief mulib ring init; see manual/mulib-ring.md for contracts and examples. */
int mulib_ring_init(mulib_ring_t*r,size_t capacity,size_t stride);
/** @brief mulib ring write; see manual/mulib-ring.md for contracts and examples. */
int mulib_ring_write(mulib_ring_t*r,const void*x);
/** @brief mulib ring read; see manual/mulib-ring.md for contracts and examples. */
int mulib_ring_read(mulib_ring_t*r,void*out);
/** @brief mulib ring free; see manual/mulib-ring.md for contracts and examples. */
void mulib_ring_free(mulib_ring_t*r);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_RING_H */

#if defined(MULIB_IMPL_RING) && !defined(MULIB_RING_IMPLEMENTED)
#define MULIB_RING_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ring_init(mulib_ring_t*r,size_t n,size_t s){if(!r||!n||!s||n>SIZE_MAX/s)return 0;memset(r,0,sizeof(*r));r->data=malloc(n*s);if(!r->data)return 0;r->capacity=n;r->stride=s;return 1;}
int mulib_ring_write(mulib_ring_t*r,const void*x){if(!r||!x||r->length==r->capacity)return 0;memcpy(r->data+((r->head+r->length)%r->capacity)*r->stride,x,r->stride);r->length++;return 1;}
int mulib_ring_read(mulib_ring_t*r,void*out){if(!r||!r->length)return 0;if(out)memcpy(out,r->data+r->head*r->stride,r->stride);r->head=(r->head+1)%r->capacity;r->length--;return 1;}
void mulib_ring_free(mulib_ring_t*r){if(r){free(r->data);memset(r,0,sizeof(*r));}}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
