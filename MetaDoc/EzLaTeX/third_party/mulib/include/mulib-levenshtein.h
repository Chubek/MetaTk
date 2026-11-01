/**
 * @file mulib-levenshtein.h
 * @brief Levenshtein byte-string distance with two-row dynamic programming.
 * @defgroup mulib_levenshtein MuLIB levenshtein
 * @{
 * @details Computes unit-cost insertion, deletion, and substitution distance
 * between C strings. The algorithm compares bytes, not Unicode graphemes.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_LEVENSHTEIN before
 * including this file. Other translation units may include it without the
 * implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_LEVENSHTEIN_H
#define MULIB_LEVENSHTEIN_H
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

  /** @brief mulib levenshtein distance; see manual/mulib-levenshtein.md for
   * contracts and examples. */
  size_t mulib_levenshtein_distance (const char *a, const char *b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_LEVENSHTEIN_H */

#if defined(MULIB_IMPL_LEVENSHTEIN) && !defined(MULIB_LEVENSHTEIN_IMPLEMENTED)
#define MULIB_LEVENSHTEIN_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_levenshtein_distance (const char *a, const char *b)
  {
    if (!a || !b)
      return SIZE_MAX;
    size_t n = strlen (a), m = strlen (b);
    if (m > SIZE_MAX / sizeof (size_t) - 1)
      return SIZE_MAX;
    size_t *p = malloc ((m + 1) * sizeof (size_t)),
           *q = malloc ((m + 1) * sizeof (size_t));
    if (!p || !q)
      {
        free (p);
        free (q);
        return SIZE_MAX;
      }
    for (size_t j = 0; j <= m; j++)
      p[j] = j;
    for (size_t i = 1; i <= n; i++)
      {
        q[0] = i;
        for (size_t j = 1; j <= m; j++)
          {
            size_t cost = (unsigned char)a[i - 1] != (unsigned char)b[j - 1];
            size_t x = p[j] + 1, y = q[j - 1] + 1, z = p[j - 1] + cost;
            q[j] = x < y ? (x < z ? x : z) : (y < z ? y : z);
          }
        size_t *t = p;
        p = q;
        q = t;
      }
    size_t result = p[m];
    free (p);
    free (q);
    return result;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
