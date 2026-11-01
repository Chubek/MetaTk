/**
 * @file mulib-split.h
 * @brief In-place token splitting into an array of field pointers.
 * @defgroup mulib_split MuLIB split
 * @{
 * @details Splits on a single byte delimiter and writes NUL terminators into
 * the input. If output capacity is exceeded, the remainder is left unsplit and
 * count equals capacity.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_SPLIT before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_SPLIT_H
#define MULIB_SPLIT_H
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

  /** @brief mulib split inplace; see manual/mulib-split.md for contracts and
   * examples. */
  size_t mulib_split_inplace (char *text, char delimiter, char **fields,
                              size_t capacity);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPLIT_H */

#if defined(MULIB_IMPL_SPLIT) && !defined(MULIB_SPLIT_IMPLEMENTED)
#define MULIB_SPLIT_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_split_inplace (char *s, char delimiter, char **out, size_t cap)
  {
    if (!s || !out || !cap)
      return 0;
    size_t n = 0;
    out[n++] = s;
    for (char *p = s; *p; p++)
      if (*p == delimiter)
        {
          if (n == cap)
            return n;
          *p = 0;
          out[n++] = p + 1;
        }
    return n;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
