/**
 * @file mulib-interpolate.h
 * @brief Scalar linear interpolation and clamped interpolation.
 * @defgroup mulib_interpolate MuLIB interpolate
 * @{
 * @details Parameter t=0 returns the first endpoint and t=1 returns the second; clamped interpolation limits t to [0,1].
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_INTERPOLATE before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_INTERPOLATE_H
#define MULIB_INTERPOLATE_H
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

/** @brief mulib interpolate linear; see manual/mulib-interpolate.md for contracts and examples. */
double mulib_interpolate_linear(double a,double b,double t);
/** @brief mulib interpolate clamped; see manual/mulib-interpolate.md for contracts and examples. */
double mulib_interpolate_clamped(double a,double b,double t);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_INTERPOLATE_H */

#if defined(MULIB_IMPL_INTERPOLATE) && !defined(MULIB_INTERPOLATE_IMPLEMENTED)
#define MULIB_INTERPOLATE_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
double mulib_interpolate_linear(double a,double b,double t){return a+(b-a)*t;}
double mulib_interpolate_clamped(double a,double b,double t){return a+(b-a)*(t<0?0:t>1?1:t);}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
