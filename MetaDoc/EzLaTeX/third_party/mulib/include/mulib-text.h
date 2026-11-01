/**
 * @file mulib-text.h
 * @brief ASCII-only whitespace trim and case conversion routines.
 * @defgroup mulib_text MuLIB text
 * @{
 * @details Works on mutable NUL-terminated strings. The trim result points
 * inside the original buffer and must not be freed separately.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_TEXT before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_TEXT_H
#define MULIB_TEXT_H
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

  /** @brief mulib text trim; see manual/mulib-text.md for contracts and
   * examples. */
  char *mulib_text_trim (char *text);
  /** @brief mulib text ascii lower; see manual/mulib-text.md for contracts and
   * examples. */
  void mulib_text_ascii_lower (char *text);
  /** @brief mulib text ascii upper; see manual/mulib-text.md for contracts and
   * examples. */
  void mulib_text_ascii_upper (char *text);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_TEXT_H */

#if defined(MULIB_IMPL_TEXT) && !defined(MULIB_TEXT_IMPLEMENTED)
#define MULIB_TEXT_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  char *
  mulib_text_trim (char *s)
  {
    if (!s)
      return NULL;
    while (isspace ((unsigned char)*s))
      s++;
    size_t n = strlen (s);
    while (n && isspace ((unsigned char)s[n - 1]))
      s[--n] = 0;
    return s;
  }
  void
  mulib_text_ascii_lower (char *s)
  {
    if (s)
      for (; *s; s++)
        if (*s >= 'A' && *s <= 'Z')
          *s = (char)(*s + 32);
  }
  void
  mulib_text_ascii_upper (char *s)
  {
    if (s)
      for (; *s; s++)
        if (*s >= 'a' && *s <= 'z')
          *s = (char)(*s - 32);
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
