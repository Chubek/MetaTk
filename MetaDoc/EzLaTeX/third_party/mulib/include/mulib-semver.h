/**
 * @file mulib-semver.h
 * @brief Semantic version triplet parsing and numeric precedence comparison.
 * @defgroup mulib_semver MuLIB semver
 * @{
 * @details Implements `MAJOR.MINOR.PATCH` only. Prerelease tags and build
 * metadata from the complete SemVer 2.0 specification are not supported.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_SEMVER before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_SEMVER_H
#define MULIB_SEMVER_H
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

  /** @brief Semantic version triplet parsing and numeric precedence
   * comparison. */
  typedef struct mulib_semver
  {
    unsigned major, minor, patch;
  } mulib_semver_t;
  /** @brief Parse three unsigned decimal components without signs, whitespace
   * or leading zeroes. Overflow and trailing data fail without modifying out.
   * No prerelease/build syntax.
   */
  int mulib_semver_parse (const char *source, mulib_semver_t *out);
  /** @brief mulib semver compare; see manual/mulib-semver.md for contracts and
   * examples. */
  int mulib_semver_compare (mulib_semver_t a, mulib_semver_t b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SEMVER_H */

#if defined(MULIB_IMPL_SEMVER) && !defined(MULIB_SEMVER_IMPLEMENTED)
#define MULIB_SEMVER_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_semver_parse (const char *s, mulib_semver_t *out)
  {
    unsigned parts[3];
    if (!s || !out)
      return 0;
    for (unsigned i = 0; i < 3; ++i)
      {
        const char *begin = s;
        unsigned value = 0;
        if (*s < '0' || *s > '9')
          return 0;
        do
          {
            unsigned digit = (unsigned)(*s - '0');
            if (value > (UINT_MAX - digit) / 10)
              return 0;
            value = value * 10 + digit;
            ++s;
          }
        while (*s >= '0' && *s <= '9');
        if (s - begin > 1 && *begin == '0')
          return 0;
        parts[i] = value;
        if (i < 2)
          {
            if (*s != '.')
              return 0;
            ++s;
          }
        else if (*s)
          return 0;
      }
    *out = (mulib_semver_t){ parts[0], parts[1], parts[2] };
    return 1;
  }
  int
  mulib_semver_compare (mulib_semver_t a, mulib_semver_t b)
  {
    if (a.major != b.major)
      return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor)
      return a.minor < b.minor ? -1 : 1;
    return a.patch == b.patch ? 0 : a.patch < b.patch ? -1 : 1;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
