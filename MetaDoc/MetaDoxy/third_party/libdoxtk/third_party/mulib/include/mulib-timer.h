/**
 * @file mulib-timer.h
 * @brief Portable elapsed-time measurements using C11 timespec_get.
 * @defgroup mulib_timer MuLIB timer
 * @{
 * @details Uses a wall clock rather than a monotonic clock because ISO C11 does not define a monotonic portable source; elapsed values can be affected by clock adjustments.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_TIMER before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_TIMER_H
#define MULIB_TIMER_H
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

/** @brief Portable elapsed-time measurements using C11 timespec_get. */
typedef struct mulib_timer { struct timespec start; } mulib_timer_t;
/** @brief mulib timer start; see manual/mulib-timer.md for contracts and examples. */
int mulib_timer_start(mulib_timer_t*t);
/** @brief mulib timer elapsed; see manual/mulib-timer.md for contracts and examples. */
double mulib_timer_elapsed(const mulib_timer_t*t);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_TIMER_H */

#if defined(MULIB_IMPL_TIMER) && !defined(MULIB_TIMER_IMPLEMENTED)
#define MULIB_TIMER_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_timer_start(mulib_timer_t*t){return t&&timespec_get(&t->start,TIME_UTC)==TIME_UTC;}
double mulib_timer_elapsed(const mulib_timer_t*t){if(!t)return NAN;struct timespec end;if(timespec_get(&end,TIME_UTC)!=TIME_UTC)return NAN;return difftime(end.tv_sec,t->start.tv_sec)+(end.tv_nsec-t->start.tv_nsec)*1e-9;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
