/**
 * @file mulib-queue.h
 * @brief A dynamically growing FIFO queue implemented with a circular buffer.
 * @defgroup mulib_queue MuLIB queue
 * @{
 * @details The queue preserves insertion order even after wraparound and relocation. Push may invalidate pointers into its storage.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_QUEUE before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_QUEUE_H
#define MULIB_QUEUE_H
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

/** @brief A dynamically growing FIFO queue implemented with a circular buffer. */
typedef struct mulib_queue { unsigned char *data; size_t head,length,capacity,stride; } mulib_queue_t;
/** @brief mulib queue init; see manual/mulib-queue.md for contracts and examples. */
void mulib_queue_init(mulib_queue_t*q,size_t stride);
/** @brief mulib queue push; see manual/mulib-queue.md for contracts and examples. */
int mulib_queue_push(mulib_queue_t*q,const void*item);
/** @brief mulib queue pop; see manual/mulib-queue.md for contracts and examples. */
int mulib_queue_pop(mulib_queue_t*q,void*out);
/** @brief mulib queue free; see manual/mulib-queue.md for contracts and examples. */
void mulib_queue_free(mulib_queue_t*q);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_QUEUE_H */

#if defined(MULIB_IMPL_QUEUE) && !defined(MULIB_QUEUE_IMPLEMENTED)
#define MULIB_QUEUE_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_queue_init(mulib_queue_t*q,size_t s){if(q){memset(q,0,sizeof(*q));q->stride=s;}}
int mulib_queue_push(mulib_queue_t*q,const void*x){if(!q||!q->stride||!x||q->length==SIZE_MAX)return 0;if(q->length==q->capacity){size_t n=q->capacity?q->capacity*2:8;if(n<q->capacity||n>SIZE_MAX/q->stride)return 0;unsigned char*p=malloc(n*q->stride);if(!p)return 0;for(size_t i=0;i<q->length;i++)memcpy(p+i*q->stride,q->data+((q->head+i)%q->capacity)*q->stride,q->stride);free(q->data);q->data=p;q->head=0;q->capacity=n;}memcpy(q->data+((q->head+q->length)%q->capacity)*q->stride,x,q->stride);q->length++;return 1;}
int mulib_queue_pop(mulib_queue_t*q,void*out){if(!q||!q->length)return 0;if(out)memcpy(out,q->data+q->head*q->stride,q->stride);q->head=(q->head+1)%q->capacity;q->length--;return 1;}
void mulib_queue_free(mulib_queue_t*q){if(q){free(q->data);memset(q,0,sizeof(*q));}}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
