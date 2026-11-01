/**
 * @file mulib-rng.h
 * @brief Deterministic 64-bit SplitMix generator; not cryptographic.
 * @defgroup mulib_rng MuLIB rng
 * @{
 * @details This generator gives reproducible pseudorandom streams. Never use
 * it for passwords, secrets, keys, or secure UUIDs.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_RNG before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_RNG_H
#define MULIB_RNG_H
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

  /** @brief Deterministic 64-bit SplitMix generator; not cryptographic. */
  typedef struct mulib_rng
  {
    uint64_t state;
  } mulib_rng_t;
  /** @brief mulib rng seed; see manual/mulib-rng.md for contracts and
   * examples. */
  void mulib_rng_seed (mulib_rng_t *r, uint64_t seed);
  /** @brief mulib rng next; see manual/mulib-rng.md for contracts and
   * examples. */
  uint64_t mulib_rng_next (mulib_rng_t *r);
  /** @brief mulib rng unit; see manual/mulib-rng.md for contracts and
   * examples. */
  double mulib_rng_unit (mulib_rng_t *r);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_RNG_H */

#if defined(MULIB_IMPL_RNG) && !defined(MULIB_RNG_IMPLEMENTED)
#define MULIB_RNG_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_rng_seed (mulib_rng_t *r, uint64_t s)
  {
    if (r)
      r->state = s;
  }
  uint64_t
  mulib_rng_next (mulib_rng_t *r)
  {
    if (!r)
      return 0;
    uint64_t z = (r->state += UINT64_C (0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C (0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C (0x94d049bb133111eb);
    return z ^ (z >> 31);
  }
  double
  mulib_rng_unit (mulib_rng_t *r)
  {
    return (mulib_rng_next (r) >> 11) * 0x1.0p-53;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
