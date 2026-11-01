/**
 * @file mulib-fraction.h
 * @brief Normalized signed rational numbers with checked arithmetic.
 * @defgroup mulib_fraction MuLIB fraction
 * @{
 * @details The representation uses positive denominators and reduces fractions by GCD. Arithmetic is deliberately conservative on overflow. The addition helper may reject some mathematically valid operations where the intermediate product exceeds signed 64-bit range.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_FRACTION before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_FRACTION_H
#define MULIB_FRACTION_H
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

/** @brief Normalized signed rational numbers with checked arithmetic. */
typedef struct mulib_fraction { int64_t num,den; } mulib_fraction_t;
/** @brief mulib fraction make; see manual/mulib-fraction.md for contracts and examples. */
int mulib_fraction_make(int64_t numerator,int64_t denominator,mulib_fraction_t*out);
/** @brief mulib fraction add; see manual/mulib-fraction.md for contracts and examples. */
int mulib_fraction_add(mulib_fraction_t a,mulib_fraction_t b,mulib_fraction_t*out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_FRACTION_H */

#if defined(MULIB_IMPL_FRACTION) && !defined(MULIB_FRACTION_IMPLEMENTED)
#define MULIB_FRACTION_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static uint64_t mulib_fraction_abs_(int64_t x){return x<0?(uint64_t)(-(x+1))+1:(uint64_t)x;}
static uint64_t mulib_fraction_gcd_(uint64_t a,uint64_t b){while(b){uint64_t t=a%b;a=b;b=t;}return a;}
int mulib_fraction_make(int64_t n,int64_t d,mulib_fraction_t*out){if(!out||!d||d==INT64_MIN)return 0;if(d<0){if(n==INT64_MIN)return 0;n=-n;d=-d;}uint64_t g=mulib_fraction_gcd_(mulib_fraction_abs_(n),(uint64_t)d);if(g){n/=(int64_t)g;d/=(int64_t)g;}*out=(mulib_fraction_t){n,d};return 1;}
int mulib_fraction_add(mulib_fraction_t a,mulib_fraction_t b,mulib_fraction_t*out){if(!out||a.den<=0||b.den<=0)return 0;/* Checked conservative bound avoids UB without requiring compiler extensions. */if(a.den>INT64_MAX/b.den)return 0;int64_t d=a.den*b.den;if(a.num==INT64_MIN||b.num==INT64_MIN)return 0;if(a.num&&(uint64_t)b.den>(uint64_t)INT64_MAX/mulib_fraction_abs_(a.num))return 0;if(b.num&&(uint64_t)a.den>(uint64_t)INT64_MAX/mulib_fraction_abs_(b.num))return 0;int64_t x=a.num*b.den,y=b.num*a.den;if((y>0&&x>INT64_MAX-y)||(y<0&&x<INT64_MIN-y))return 0;return mulib_fraction_make(x+y,d,out);}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
