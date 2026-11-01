/**
 * @file mulib-stats.h
 * @brief Streaming sample statistics using Welford numerical updates.
 * @defgroup mulib_stats MuLIB stats
 * @{
 * @details Variance is the sample variance (denominator n-1); fewer than two samples returns NAN.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_STATS before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_STATS_H
#define MULIB_STATS_H
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

/** @brief Streaming sample statistics using Welford numerical updates. */
typedef struct mulib_stats { size_t count; double mean,m2,min,max; } mulib_stats_t;
/** @brief mulib stats init; see manual/mulib-stats.md for contracts and examples. */
void mulib_stats_init(mulib_stats_t*s);
/** @brief mulib stats add; see manual/mulib-stats.md for contracts and examples. */
void mulib_stats_add(mulib_stats_t*s,double value);
/** @brief mulib stats variance; see manual/mulib-stats.md for contracts and examples. */
double mulib_stats_variance(const mulib_stats_t*s);
/** @brief mulib stats stddev; see manual/mulib-stats.md for contracts and examples. */
double mulib_stats_stddev(const mulib_stats_t*s);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_STATS_H */

#if defined(MULIB_IMPL_STATS) && !defined(MULIB_STATS_IMPLEMENTED)
#define MULIB_STATS_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_stats_init(mulib_stats_t*s){if(s){memset(s,0,sizeof(*s));s->min=INFINITY;s->max=-INFINITY;}}
void mulib_stats_add(mulib_stats_t*s,double x){if(!s||s->count==SIZE_MAX)return;s->count++;double d=x-s->mean;s->mean+=d/(double)s->count;s->m2+=d*(x-s->mean);if(x<s->min)s->min=x;if(x>s->max)s->max=x;}
double mulib_stats_variance(const mulib_stats_t*s){return s&&s->count>1?s->m2/(double)(s->count-1):NAN;}
double mulib_stats_stddev(const mulib_stats_t*s){return sqrt(mulib_stats_variance(s));}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
