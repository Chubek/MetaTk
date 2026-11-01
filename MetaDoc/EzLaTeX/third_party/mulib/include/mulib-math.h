/**
 * @file mulib-math.h
 * @brief Integer arithmetic utilities: GCD, LCM and bounded integer power.
 * @defgroup mulib_math MuLIB math
 * @{
 * @details GCD(0,0) returns 0. Overflow-sensitive functions return zero
 * without writing their output when multiplication would overflow.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_MATH before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_MATH_H
#define MULIB_MATH_H
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

  /** @brief mulib math gcd; see manual/mulib-math.md for contracts and
   * examples. */
  uint64_t mulib_math_gcd (uint64_t a, uint64_t b);
  /** @brief mulib math lcm; see manual/mulib-math.md for contracts and
   * examples. */
  int mulib_math_lcm (uint64_t a, uint64_t b, uint64_t *out);
  /** @brief mulib math pow u64; see manual/mulib-math.md for contracts and
   * examples. */
  int mulib_math_pow_u64 (uint64_t base, unsigned exponent, uint64_t *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_MATH_H */

#if defined(MULIB_IMPL_MATH) && !defined(MULIB_MATH_IMPLEMENTED)
#define MULIB_MATH_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  uint64_t
  mulib_math_gcd (uint64_t a, uint64_t b)
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
  mulib_math_lcm (uint64_t a, uint64_t b, uint64_t *out)
  {
    if (!out)
      return 0;
    if (!a || !b)
      {
        *out = 0;
        return 1;
      }
    uint64_t c = a / mulib_math_gcd (a, b);
    if (b > UINT64_MAX / c)
      return 0;
    *out = c * b;
    return 1;
  }
  int
  mulib_math_pow_u64 (uint64_t x, unsigned e, uint64_t *out)
  {
    if (!out)
      return 0;
    uint64_t a = 1;
    while (e)
      {
        if (e & 1)
          {
            if (x && a > UINT64_MAX / x)
              return 0;
            a *= x;
          }
        e >>= 1;
        if (e)
          {
            if (x && x > UINT64_MAX / x)
              return 0;
            x *= x;
          }
      }
    *out = a;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
