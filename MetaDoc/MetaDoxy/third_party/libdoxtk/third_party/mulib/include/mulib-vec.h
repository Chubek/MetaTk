/**
 * @file mulib-vec.h
 * @brief An untyped, byte-stride dynamic vector with amortized growth.
 * @defgroup mulib_vec MuLIB vec
 * @{
 * @details Elements are copied as raw bytes. Store trivially copyable C objects and manage any resources inside elements yourself.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_VEC before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_VEC_H
#define MULIB_VEC_H
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

/** @brief An untyped, byte-stride dynamic vector with amortized growth. */
typedef struct mulib_vec { unsigned char *data; size_t length, capacity, stride; } mulib_vec_t;
/** @brief mulib vec init; see manual/mulib-vec.md for contracts and examples. */
void mulib_vec_init(mulib_vec_t *v, size_t element_size);
/** @brief mulib vec reserve; see manual/mulib-vec.md for contracts and examples. */
int mulib_vec_reserve(mulib_vec_t *v, size_t elements);
/** @brief mulib vec push; see manual/mulib-vec.md for contracts and examples. */
int mulib_vec_push(mulib_vec_t *v, const void *element);
/** @brief mulib vec at; see manual/mulib-vec.md for contracts and examples. */
void *mulib_vec_at(mulib_vec_t *v, size_t index);
/** @brief mulib vec pop; see manual/mulib-vec.md for contracts and examples. */
int mulib_vec_pop(mulib_vec_t *v, void *out_element);
/** @brief mulib vec clear; see manual/mulib-vec.md for contracts and examples. */
void mulib_vec_clear(mulib_vec_t *v);
/** @brief mulib vec free; see manual/mulib-vec.md for contracts and examples. */
void mulib_vec_free(mulib_vec_t *v);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_VEC_H */

#if defined(MULIB_IMPL_VEC) && !defined(MULIB_VEC_IMPLEMENTED)
#define MULIB_VEC_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_vec_init(mulib_vec_t *v,size_t stride){if(v){v->data=NULL;v->length=v->capacity=0;v->stride=stride;}}
int mulib_vec_reserve(mulib_vec_t *v,size_t n){if(!v||!v->stride||n>SIZE_MAX/v->stride)return 0;if(n<=v->capacity)return 1;size_t cap=v->capacity?v->capacity:8;while(cap<n){if(cap>SIZE_MAX/2){cap=n;break;}cap*=2;}if(cap>SIZE_MAX/v->stride)return 0;void*p=realloc(v->data,cap*v->stride);if(!p)return 0;v->data=p;v->capacity=cap;return 1;}
int mulib_vec_push(mulib_vec_t *v,const void *e){if(!v||!e||v->length==SIZE_MAX||!mulib_vec_reserve(v,v->length+1))return 0;memcpy(v->data+v->length*v->stride,e,v->stride);++v->length;return 1;}
void *mulib_vec_at(mulib_vec_t *v,size_t i){return v&&i<v->length?v->data+i*v->stride:NULL;}
int mulib_vec_pop(mulib_vec_t *v,void *out){if(!v||!v->length)return 0;--v->length;if(out)memcpy(out,v->data+v->length*v->stride,v->stride);return 1;}
void mulib_vec_clear(mulib_vec_t *v){if(v)v->length=0;}
void mulib_vec_free(mulib_vec_t *v){if(v){free(v->data);mulib_vec_init(v,v->stride);}}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
