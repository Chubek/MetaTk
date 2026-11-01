/**
 * @file mulib-set.h
 * @brief Unique set of signed 64-bit integers.
 * @defgroup mulib_set MuLIB set
 * @{
 * @details Insert duplicates succeed without increasing the set size. Remove does not preserve enumeration order.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_SET before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_SET_H
#define MULIB_SET_H
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

/** @brief Unique set of signed 64-bit integers. */
typedef struct mulib_set { int64_t *items; size_t length,capacity; } mulib_set_t;
/** @brief mulib set init; see manual/mulib-set.md for contracts and examples. */
void mulib_set_init(mulib_set_t*s);
/** @brief mulib set add; see manual/mulib-set.md for contracts and examples. */
int mulib_set_add(mulib_set_t*s,int64_t x);
/** @brief mulib set contains; see manual/mulib-set.md for contracts and examples. */
int mulib_set_contains(const mulib_set_t*s,int64_t x);
/** @brief mulib set remove; see manual/mulib-set.md for contracts and examples. */
int mulib_set_remove(mulib_set_t*s,int64_t x);
/** @brief mulib set free; see manual/mulib-set.md for contracts and examples. */
void mulib_set_free(mulib_set_t*s);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SET_H */

#if defined(MULIB_IMPL_SET) && !defined(MULIB_SET_IMPLEMENTED)
#define MULIB_SET_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_set_init(mulib_set_t*s){if(s)memset(s,0,sizeof(*s));}
int mulib_set_contains(const mulib_set_t*s,int64_t x){if(!s)return 0;for(size_t i=0;i<s->length;i++)if(s->items[i]==x)return 1;return 0;}
int mulib_set_add(mulib_set_t*s,int64_t x){if(!s)return 0;if(mulib_set_contains(s,x))return 1;if(s->length==s->capacity){size_t n=s->capacity?s->capacity*2:8;if(n<s->capacity||n>SIZE_MAX/sizeof(int64_t))return 0;void*p=realloc(s->items,n*sizeof(int64_t));if(!p)return 0;s->items=p;s->capacity=n;}s->items[s->length++]=x;return 1;}
int mulib_set_remove(mulib_set_t*s,int64_t x){if(!s)return 0;for(size_t i=0;i<s->length;i++)if(s->items[i]==x){s->items[i]=s->items[--s->length];return 1;}return 0;}
void mulib_set_free(mulib_set_t*s){if(s){free(s->items);memset(s,0,sizeof(*s));}}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
