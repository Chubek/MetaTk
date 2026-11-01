/**
 * @file mulib-adler32.h
 * @brief Adler-32 checksum with streaming seed support.
 * @defgroup mulib_adler32 MuLIB adler32
 * @{
 * @details Adler-32 is a fast corruption check, not a cryptographic integrity
 * code. Initialize incremental checksums with seed 1.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ADLER32 before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_ADLER32_H
#define MULIB_ADLER32_H
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

  /** @brief mulib adler32 update; see manual/mulib-adler32.md for contracts
   * and examples. */
  uint32_t mulib_adler32_update (uint32_t seed, const void *data,
                                 size_t length);
  /** @brief mulib adler32; see manual/mulib-adler32.md for contracts and
   * examples. */
  uint32_t mulib_adler32 (const void *data, size_t length);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ADLER32_H */

#if defined(MULIB_IMPL_ADLER32) && !defined(MULIB_ADLER32_IMPLEMENTED)
#define MULIB_ADLER32_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  uint32_t
  mulib_adler32_update (uint32_t seed, const void *p, size_t n)
  {
    if (!p && n)
      return seed;
    uint32_t a = seed & 65535u, b = seed >> 16;
    const unsigned char *s = p;
    for (size_t i = 0; i < n; i++)
      {
        a = (a + s[i]) % 65521u;
        b = (b + a) % 65521u;
      }
    return (b << 16) | a;
  }
  uint32_t
  mulib_adler32 (const void *p, size_t n)
  {
    return mulib_adler32_update (1, p, n);
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
