/**
 * @file mulib-prime.h
 * @brief Primality testing and next-prime search for 64-bit integers.
 * @defgroup mulib_prime MuLIB prime
 * @{
 * @details Deterministic Miller-Rabin covers the full uint64_t domain using
 * portable overflow-safe modular arithmetic. This module is not a
 * cryptographic or constant-time prime generator.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_PRIME before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_PRIME_H
#define MULIB_PRIME_H
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

  /** @brief Deterministic uint64_t primality via seven Miller-Rabin bases; not
   * constant-time. */
  int mulib_prime_is_prime (uint64_t number);
  /** @brief mulib prime next; see manual/mulib-prime.md for contracts and
   * examples. */
  uint64_t mulib_prime_next (uint64_t number);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRIME_H */

#if defined(MULIB_IMPL_PRIME) && !defined(MULIB_PRIME_IMPLEMENTED)
#define MULIB_PRIME_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  static uint64_t
  mulib_prime_addmod_ (uint64_t a, uint64_t b, uint64_t m)
  {
    return a >= m - b ? a - (m - b) : a + b;
  }
  static uint64_t
  mulib_prime_mulmod_ (uint64_t a, uint64_t b, uint64_t m)
  {
    uint64_t value = 0;
    while (b)
      {
        if (b & 1)
          value = mulib_prime_addmod_ (value, a, m);
        b >>= 1;
        if (b)
          a = mulib_prime_addmod_ (a, a, m);
      }
    return value;
  }
  static uint64_t
  mulib_prime_powmod_ (uint64_t a, uint64_t exponent, uint64_t m)
  {
    uint64_t value = 1;
    while (exponent)
      {
        if (exponent & 1)
          value = mulib_prime_mulmod_ (value, a, m);
        exponent >>= 1;
        if (exponent)
          a = mulib_prime_mulmod_ (a, a, m);
      }
    return value;
  }
  int
  mulib_prime_is_prime (uint64_t n)
  {
    /* These seven Miller-Rabin bases are deterministic over the uint64_t
     * domain. Add/double modular multiplication needs no nonstandard 128-bit
     * integer. */
    static const uint64_t bases[]
        = { 2, 325, 9375, 28178, 450775, 9780504, 1795265022 };
    static const unsigned small[]
        = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37 };
    uint64_t d;
    unsigned shifts = 0;
    if (n < 2)
      return 0;
    for (size_t i = 0; i < sizeof (small) / sizeof (*small); ++i)
      if (n % small[i] == 0)
        return n == small[i];
    d = n - 1;
    while (!(d & 1))
      {
        d >>= 1;
        ++shifts;
      }
    for (size_t i = 0; i < sizeof (bases) / sizeof (*bases); ++i)
      {
        uint64_t a = bases[i] % n, x;
        unsigned j;
        if (!a)
          continue;
        x = mulib_prime_powmod_ (a, d, n);
        if (x == 1 || x == n - 1)
          continue;
        for (j = 1; j < shifts; ++j)
          {
            x = mulib_prime_mulmod_ (x, x, n);
            if (x == n - 1)
              break;
          }
        if (j == shifts)
          return 0;
      }
    return 1;
  }
  uint64_t
  mulib_prime_next (uint64_t n)
  {
    if (n <= 2)
      return 2;
    if (!(n & 1))
      n++;
    while (n)
      {
        if (mulib_prime_is_prime (n))
          return n;
        if (n > UINT64_MAX - 2)
          break;
        n += 2;
      }
    return 0;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
