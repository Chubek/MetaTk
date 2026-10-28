/**
 * @file mulib-movingavg.h
 * @brief Constant-memory fixed-window moving average over doubles.
 * @defgroup mulib_movingavg MuLIB movingavg
 * @{
 * @details Uses a circular window of exactly `capacity` recent values and updates its sum in constant time. Very long streams may accumulate floating-point summation drift.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_MOVINGAVG before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_MOVINGAVG_H
#define MULIB_MOVINGAVG_H
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

/** @brief Constant-memory fixed-window moving average over doubles. */
typedef struct mulib_movingavg { double *values,sum; size_t capacity,length,position; } mulib_movingavg_t;
/** @brief mulib movingavg init; see manual/mulib-movingavg.md for contracts and examples. */
int mulib_movingavg_init(mulib_movingavg_t*m,size_t window);
/** @brief mulib movingavg push; see manual/mulib-movingavg.md for contracts and examples. */
double mulib_movingavg_push(mulib_movingavg_t*m,double value);
/** @brief mulib movingavg free; see manual/mulib-movingavg.md for contracts and examples. */
void mulib_movingavg_free(mulib_movingavg_t*m);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_MOVINGAVG_H */

#if defined(MULIB_IMPL_MOVINGAVG) && !defined(MULIB_MOVINGAVG_IMPLEMENTED)
#define MULIB_MOVINGAVG_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_movingavg_init(mulib_movingavg_t*m,size_t n){if(!m||!n||n>SIZE_MAX/sizeof(double))return 0;memset(m,0,sizeof(*m));m->values=calloc(n,sizeof(double));if(!m->values)return 0;m->capacity=n;return 1;}
double mulib_movingavg_push(mulib_movingavg_t*m,double x){if(!m||!m->values)return NAN;if(m->length==m->capacity)m->sum-=m->values[m->position];else m->length++;m->values[m->position]=x;m->sum+=x;m->position=(m->position+1)%m->capacity;return m->sum/m->length;}
void mulib_movingavg_free(mulib_movingavg_t*m){if(m){free(m->values);memset(m,0,sizeof(*m));}}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
