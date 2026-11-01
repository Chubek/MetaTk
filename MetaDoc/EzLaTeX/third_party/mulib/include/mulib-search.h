/**
 * @file mulib-search.h
 * @brief Substring finding via Knuth-Morris-Pratt with explicit byte lengths.
 * @defgroup mulib_search MuLIB search
 * @{
 * @details Returns SIZE_MAX when no match is found or allocation fails, and 0
 * for an empty needle. Runs in O(haystack length + needle length) time.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_SEARCH before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_SEARCH_H
#define MULIB_SEARCH_H
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

  /** @brief mulib search kmp; see manual/mulib-search.md for contracts and
   * examples. */
  size_t mulib_search_kmp (const void *haystack, size_t hay_len,
                           const void *needle, size_t needle_len);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SEARCH_H */

#if defined(MULIB_IMPL_SEARCH) && !defined(MULIB_SEARCH_IMPLEMENTED)
#define MULIB_SEARCH_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_search_kmp (const void *hay, size_t hn, const void *needle, size_t nn)
  {
    if (!nn)
      return 0;
    if (!hay || !needle || nn > hn || nn > SIZE_MAX / sizeof (size_t))
      return SIZE_MAX;
    const unsigned char *h = hay, *p = needle;
    size_t *fail = calloc (nn, sizeof (size_t));
    if (!fail)
      return SIZE_MAX;
    for (size_t i = 1, j = 0; i < nn; i++)
      {
        while (j && p[i] != p[j])
          j = fail[j - 1];
        if (p[i] == p[j])
          j++;
        fail[i] = j;
      }
    size_t res = SIZE_MAX;
    for (size_t i = 0, j = 0; i < hn; i++)
      {
        while (j && h[i] != p[j])
          j = fail[j - 1];
        if (h[i] == p[j])
          j++;
        if (j == nn)
          {
            res = i + 1 - nn;
            break;
          }
      }
    free (fail);
    return res;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
