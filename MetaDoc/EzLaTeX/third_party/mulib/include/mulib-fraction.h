/**
 * @file mulib-fraction.h
 * @brief Normalized signed rational numbers with checked arithmetic.
 * @defgroup mulib_fraction MuLIB fraction
 * @{
 * @details The representation uses positive denominators and reduces fractions
 * by GCD. Arithmetic is deliberately conservative on overflow. The addition
 * helper may reject some mathematically valid operations where the
 * intermediate product exceeds signed 64-bit range.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_FRACTION before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_FRACTION_H
#define MULIB_FRACTION_H
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

  /** @brief Normalized signed rational numbers with checked arithmetic. */
  typedef struct mulib_fraction
  {
    int64_t num, den;
  } mulib_fraction_t;
  /** @brief mulib fraction make; see manual/mulib-fraction.md for contracts
   * and examples. */
  int mulib_fraction_make (int64_t numerator, int64_t denominator,
                           mulib_fraction_t *out);
  /** @brief mulib fraction add; see manual/mulib-fraction.md for contracts and
   * examples. */
  int mulib_fraction_add (mulib_fraction_t a, mulib_fraction_t b,
                          mulib_fraction_t *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_FRACTION_H */

#if defined(MULIB_IMPL_FRACTION) && !defined(MULIB_FRACTION_IMPLEMENTED)
#define MULIB_FRACTION_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  static uint64_t
  mulib_fraction_abs_ (int64_t x)
  {
    return x < 0 ? (uint64_t)(-(x + 1)) + 1 : (uint64_t)x;
  }
  static uint64_t
  mulib_fraction_gcd_ (uint64_t a, uint64_t b)
  {
    while (b)
      {
        uint64_t t = a % b;
        a = b;
        b = t;
      }
    return a;
  }
  int
  mulib_fraction_make (int64_t n, int64_t d, mulib_fraction_t *out)
  {
    uint64_t un, ud, g;
    int negative;
    int64_t numerator;
    if (!out || !d)
      return 0;
    un = mulib_fraction_abs_ (n);
    ud = mulib_fraction_abs_ (d);
    negative = (n < 0) != (d < 0);
    g = mulib_fraction_gcd_ (un, ud);
    un /= g;
    ud /= g;
    if (ud > INT64_MAX || un > (uint64_t)INT64_MAX + (negative ? 1u : 0u))
      return 0;
    if (!negative)
      numerator = (int64_t)un;
    else if (un == (uint64_t)INT64_MAX + 1u)
      numerator = INT64_MIN;
    else
      numerator = -(int64_t)un;
    *out = (mulib_fraction_t){ numerator, (int64_t)ud };
    return 1;
  }
  static int
  mulib_fraction_scale_ (int64_t n, int64_t scale, int64_t *out)
  {
    if ((n > 0 && n > INT64_MAX / scale) || (n < 0 && n < INT64_MIN / scale))
      return 0;
    *out = n * scale;
    return 1;
  }
  int
  mulib_fraction_add (mulib_fraction_t a, mulib_fraction_t b,
                      mulib_fraction_t *out)
  {
    int64_t x, y, left, right, d;
    uint64_t g;
    if (!out || a.den <= 0 || b.den <= 0)
      return 0;
    g = mulib_fraction_gcd_ ((uint64_t)a.den, (uint64_t)b.den);
    left = b.den / (int64_t)g;
    right = a.den / (int64_t)g;
    /* Reduce common denominator factors before checked multiplication. */
    if (a.den > INT64_MAX / left || !mulib_fraction_scale_ (a.num, left, &x)
        || !mulib_fraction_scale_ (b.num, right, &y))
      return 0;
    d = a.den * left;
    if ((y > 0 && x > INT64_MAX - y) || (y < 0 && x < INT64_MIN - y))
      return 0;
    return mulib_fraction_make (x + y, d, out);
  }

#ifdef __cplusplus
}
#endif
#endif /* implementation */
