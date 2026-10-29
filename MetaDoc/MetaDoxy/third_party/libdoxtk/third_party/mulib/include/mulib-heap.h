/**
 * @file mulib-heap.h
 * @brief A binary min-heap of signed 64-bit integers.
 * @defgroup mulib_heap MuLIB heap
 * @{
 * @details The minimum value is returned first. Push and pop take logarithmic time; memory is owned by the heap.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_HEAP before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_HEAP_H
#define MULIB_HEAP_H
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

/** @brief A binary min-heap of signed 64-bit integers. */
typedef struct mulib_heap { int64_t *items; size_t length,capacity; } mulib_heap_t;
/** @brief mulib heap init; see manual/mulib-heap.md for contracts and examples. */
void mulib_heap_init(mulib_heap_t*h);
/** @brief mulib heap push; see manual/mulib-heap.md for contracts and examples. */
int mulib_heap_push(mulib_heap_t*h,int64_t value);
/** @brief mulib heap pop; see manual/mulib-heap.md for contracts and examples. */
int mulib_heap_pop(mulib_heap_t*h,int64_t*out);
/** @brief mulib heap free; see manual/mulib-heap.md for contracts and examples. */
void mulib_heap_free(mulib_heap_t*h);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_HEAP_H */

#if defined(MULIB_IMPL_HEAP) && !defined(MULIB_HEAP_IMPLEMENTED)
#define MULIB_HEAP_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_heap_init(mulib_heap_t*h){if(h)memset(h,0,sizeof(*h));}
int mulib_heap_push(mulib_heap_t*h,int64_t x){if(!h)return 0;if(h->length==h->capacity){size_t n=h->capacity?h->capacity*2:8;if(n<h->capacity||n>SIZE_MAX/sizeof(int64_t))return 0;void*p=realloc(h->items,n*sizeof(int64_t));if(!p)return 0;h->items=p;h->capacity=n;}size_t i=h->length++;while(i){size_t p=(i-1)/2;if(h->items[p]<=x)break;h->items[i]=h->items[p];i=p;}h->items[i]=x;return 1;}
int mulib_heap_pop(mulib_heap_t*h,int64_t*out){if(!h||!h->length)return 0;if(out)*out=h->items[0];int64_t x=h->items[--h->length];if(h->length){size_t i=0;while(i<h->length/2){size_t c=i*2+1;if(c+1<h->length&&h->items[c+1]<h->items[c])c++;if(h->items[c]>=x)break;h->items[i]=h->items[c];i=c;}h->items[i]=x;}return 1;}
void mulib_heap_free(mulib_heap_t*h){if(h){free(h->items);memset(h,0,sizeof(*h));}}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
