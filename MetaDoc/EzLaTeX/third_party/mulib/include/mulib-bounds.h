/**
 * @file mulib-bounds.h
 * @brief Axis-aligned bounding boxes and intersection utilities.
 * @defgroup mulib_bounds MuLIB bounds
 * @{
 * @details All boundaries are inclusive. Callers must provide normalized boxes
 * with min coordinates less than or equal to max coordinates.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_BOUNDS before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_BOUNDS_H
#define MULIB_BOUNDS_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Axis-aligned bounding boxes and intersection utilities. */
  typedef struct mulib_bounds
  {
    double minx, miny, maxx, maxy;
  } mulib_bounds_t;
  /** @brief mulib bounds contains; see manual/mulib-bounds.md for contracts
   * and examples. */
  int mulib_bounds_contains (mulib_bounds_t b, double x, double y);
  /** @brief mulib bounds intersects; see manual/mulib-bounds.md for contracts
   * and examples. */
  int mulib_bounds_intersects (mulib_bounds_t a, mulib_bounds_t b);
  /** @brief mulib bounds union; see manual/mulib-bounds.md for contracts and
   * examples. */
  mulib_bounds_t mulib_bounds_union (mulib_bounds_t a, mulib_bounds_t b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_BOUNDS_H */

#if defined(MULIB_IMPL_BOUNDS) && !defined(MULIB_BOUNDS_IMPLEMENTED)
#define MULIB_BOUNDS_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_bounds_contains (mulib_bounds_t b, double x, double y)
  {
    return x >= b.minx && x <= b.maxx && y >= b.miny && y <= b.maxy;
  }
  int
  mulib_bounds_intersects (mulib_bounds_t a, mulib_bounds_t b)
  {
    return a.maxx >= b.minx && b.maxx >= a.minx && a.maxy >= b.miny
           && b.maxy >= a.miny;
  }
  mulib_bounds_t
  mulib_bounds_union (mulib_bounds_t a, mulib_bounds_t b)
  {
    return (mulib_bounds_t){ fmin (a.minx, b.minx), fmin (a.miny, b.miny),
                             fmax (a.maxx, b.maxx), fmax (a.maxy, b.maxy) };
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
