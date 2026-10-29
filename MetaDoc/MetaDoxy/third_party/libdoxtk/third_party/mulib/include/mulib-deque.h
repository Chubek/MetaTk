/**
 * @file mulib-deque.h
 * @brief Double-ended FIFO container with a circular array.
 * @defgroup mulib_deque MuLIB deque
 * @{
 * @details O(1) amortized insertion and removal at either end. The deque owns storage but does not own pointers stored in elements.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_DEQUE before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_DEQUE_H
#define MULIB_DEQUE_H
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

/** @brief Double-ended FIFO container with a circular array. */
typedef struct mulib_deque { unsigned char *data; size_t head,length,capacity,stride; } mulib_deque_t;
/** @brief mulib deque init; see manual/mulib-deque.md for contracts and examples. */
void mulib_deque_init(mulib_deque_t*d,size_t stride);
/** @brief mulib deque push front; see manual/mulib-deque.md for contracts and examples. */
int mulib_deque_push_front(mulib_deque_t*d,const void*x);
/** @brief mulib deque push back; see manual/mulib-deque.md for contracts and examples. */
int mulib_deque_push_back(mulib_deque_t*d,const void*x);
/** @brief mulib deque pop front; see manual/mulib-deque.md for contracts and examples. */
int mulib_deque_pop_front(mulib_deque_t*d,void*out);
/** @brief mulib deque pop back; see manual/mulib-deque.md for contracts and examples. */
int mulib_deque_pop_back(mulib_deque_t*d,void*out);
/** @brief mulib deque free; see manual/mulib-deque.md for contracts and examples. */
void mulib_deque_free(mulib_deque_t*d);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_DEQUE_H */

#if defined(MULIB_IMPL_DEQUE) && !defined(MULIB_DEQUE_IMPLEMENTED)
#define MULIB_DEQUE_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_deque_init(mulib_deque_t*d,size_t s){if(d){memset(d,0,sizeof(*d));d->stride=s;}}
static int mulib_deque_grow_(mulib_deque_t*d){if(d->length<d->capacity)return 1;size_t n=d->capacity?d->capacity*2:8;if(!d->stride||n<d->capacity||n>SIZE_MAX/d->stride)return 0;unsigned char*p=malloc(n*d->stride);if(!p)return 0;for(size_t i=0;i<d->length;i++)memcpy(p+i*d->stride,d->data+((d->head+i)%d->capacity)*d->stride,d->stride);free(d->data);d->data=p;d->head=0;d->capacity=n;return 1;}
int mulib_deque_push_back(mulib_deque_t*d,const void*x){if(!d||!x||!mulib_deque_grow_(d))return 0;memcpy(d->data+((d->head+d->length)%d->capacity)*d->stride,x,d->stride);d->length++;return 1;}
int mulib_deque_push_front(mulib_deque_t*d,const void*x){if(!d||!x||!mulib_deque_grow_(d))return 0;d->head=(d->head+d->capacity-1)%d->capacity;memcpy(d->data+d->head*d->stride,x,d->stride);d->length++;return 1;}
int mulib_deque_pop_front(mulib_deque_t*d,void*out){if(!d||!d->length)return 0;if(out)memcpy(out,d->data+d->head*d->stride,d->stride);d->head=(d->head+1)%d->capacity;d->length--;return 1;}
int mulib_deque_pop_back(mulib_deque_t*d,void*out){if(!d||!d->length)return 0;d->length--;if(out)memcpy(out,d->data+((d->head+d->length)%d->capacity)*d->stride,d->stride);return 1;}
void mulib_deque_free(mulib_deque_t*d){if(d){free(d->data);memset(d,0,sizeof(*d));}}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
