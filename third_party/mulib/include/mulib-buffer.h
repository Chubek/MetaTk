/**
 * @file mulib-buffer.h
 * @brief Dynamically growing binary byte buffer.
 * @defgroup mulib_buffer MuLIB buffer
 * @{
 * @details Unlike strbuf, buffer permits zero bytes and does not append a string terminator.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_BUFFER before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_BUFFER_H
#define MULIB_BUFFER_H
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

/** @brief Dynamically growing binary byte buffer. */
typedef struct mulib_buffer { unsigned char *data; size_t length,capacity; } mulib_buffer_t;
/** @brief mulib buffer init; see manual/mulib-buffer.md for contracts and examples. */
void mulib_buffer_init(mulib_buffer_t*b);
/** @brief mulib buffer append; see manual/mulib-buffer.md for contracts and examples. */
int mulib_buffer_append(mulib_buffer_t*b,const void*data,size_t n);
/** @brief mulib buffer remove; see manual/mulib-buffer.md for contracts and examples. */
int mulib_buffer_remove(mulib_buffer_t*b,size_t offset,size_t n);
/** @brief mulib buffer free; see manual/mulib-buffer.md for contracts and examples. */
void mulib_buffer_free(mulib_buffer_t*b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_BUFFER_H */

#if defined(MULIB_IMPL_BUFFER) && !defined(MULIB_BUFFER_IMPLEMENTED)
#define MULIB_BUFFER_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_buffer_init(mulib_buffer_t*b){if(b)memset(b,0,sizeof(*b));}
int mulib_buffer_append(mulib_buffer_t*b,const void*p,size_t n){if(!b||(!p&&n)||n>SIZE_MAX-b->length)return 0;size_t want=b->length+n;if(want>b->capacity){size_t cap=b->capacity?b->capacity:64;while(cap<want){if(cap>SIZE_MAX/2){cap=want;break;}cap*=2;}void*q=realloc(b->data,cap);if(!q)return 0;b->data=q;b->capacity=cap;}if(n)memmove(b->data+b->length,p,n);b->length=want;return 1;}
int mulib_buffer_remove(mulib_buffer_t*b,size_t off,size_t n){if(!b||off>b->length||n>b->length-off)return 0;memmove(b->data+off,b->data+off+n,b->length-off-n);b->length-=n;return 1;}
void mulib_buffer_free(mulib_buffer_t*b){if(b){free(b->data);memset(b,0,sizeof(*b));}}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
