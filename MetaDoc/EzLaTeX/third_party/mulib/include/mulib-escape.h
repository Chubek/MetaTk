/**
 * @file mulib-escape.h
 * @brief C-style escaping and unescaping of byte strings.
 * @defgroup mulib_escape MuLIB escape
 * @{
 * @details Escapes standard control bytes newline, tab, carriage return, slash
 * and double quotes. This is not a full C lexical escape parser; hex and octal
 * escapes are unsupported.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ESCAPE before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_ESCAPE_H
#define MULIB_ESCAPE_H
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

  /** @brief mulib escape c; see manual/mulib-escape.md for contracts and
   * examples. */
  size_t mulib_escape_c (const char *source, char *out, size_t capacity);
  /** @brief mulib escape unc; see manual/mulib-escape.md for contracts and
   * examples. */
  int mulib_escape_unc (const char *source, char *out, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ESCAPE_H */

#if defined(MULIB_IMPL_ESCAPE) && !defined(MULIB_ESCAPE_IMPLEMENTED)
#define MULIB_ESCAPE_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_escape_c (const char *s, char *out, size_t cap)
  {
    if (!s || !out)
      return 0;
    size_t j = 0;
    for (; *s; s++)
      {
        char c = *s;
        char code = c == '\n'   ? 'n'
                    : c == '\t' ? 't'
                    : c == '\r' ? 'r'
                    : c == '\\' ? '\\'
                    : c == '"'  ? '"'
                                : 0;
        size_t need = code ? 2 : 1;
        if (j > cap || need >= cap - j)
          return 0;
        if (code)
          out[j++] = '\\';
        out[j++] = code ? code : c;
      }
    if (j >= cap)
      return 0;
    out[j] = 0;
    return j;
  }
  int
  mulib_escape_unc (const char *s, char *out, size_t cap)
  {
    if (!s || !out)
      return 0;
    size_t j = 0;
    while (*s)
      {
        char c = *s++;
        if (c == '\\')
          {
            if (!*s)
              return 0;
            c = *s++;
            if (c == 'n')
              c = '\n';
            else if (c == 't')
              c = '\t';
            else if (c == 'r')
              c = '\r';
            else if (c != '\\' && c != '"')
              return 0;
          }
        if (j + 1 >= cap)
          return 0;
        out[j++] = c;
      }
    if (j >= cap)
      return 0;
    out[j] = 0;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
