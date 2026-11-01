/**
 * @file mulib-replace.h
 * @brief Allocate a string with every nonoverlapping occurrence replaced.
 * @defgroup mulib_replace MuLIB replace
 * @{
 * @details Performs left-to-right non-overlapping substitutions. Returns a
 * newly allocated string that the caller must free. Empty search strings are
 * refused.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_REPLACE before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_REPLACE_H
#define MULIB_REPLACE_H
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

  /** @brief mulib replace all; see manual/mulib-replace.md for contracts and
   * examples. */
  char *mulib_replace_all (const char *source, const char *needle,
                           const char *replacement);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_REPLACE_H */

#if defined(MULIB_IMPL_REPLACE) && !defined(MULIB_REPLACE_IMPLEMENTED)
#define MULIB_REPLACE_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  char *
  mulib_replace_all (const char *s, const char *needle, const char *rep)
  {
    if (!s || !needle || !rep || !*needle)
      return NULL;
    size_t a = strlen (s), b = strlen (needle), c = strlen (rep), count = 0;
    for (const char *p = s; (p = strstr (p, needle)); p += b)
      count++;
    size_t len = a;
    if (c >= b)
      {
        if (count && (c - b) > (SIZE_MAX - a - 1) / count)
          return NULL;
        len += count * (c - b);
      }
    else
      len -= count * (b - c);
    char *out = malloc (len + 1);
    if (!out)
      return NULL;
    char *w = out;
    const char *p = s, *q;
    while ((q = strstr (p, needle)))
      {
        size_t n = (size_t)(q - p);
        memcpy (w, p, n);
        w += n;
        memcpy (w, rep, c);
        w += c;
        p = q + b;
      }
    strcpy (w, p);
    return out;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
