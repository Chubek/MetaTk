/**
 * @file mulib-csv.h
 * @brief Streaming CSV row parser for quoted comma-separated fields.
 * @defgroup mulib_csv MuLIB csv
 * @{
 * @details Parses escaped double quotes (`""`) and quoted delimiters/newlines.
 * Row fields are owned temporarily by the parser and are valid only for the
 * duration of a callback invocation. This is a lightweight dialect, not a
 * dialect autodetector.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_CSV before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_CSV_H
#define MULIB_CSV_H
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

  /** @brief Streaming CSV row parser for quoted comma-separated fields. */
  typedef int (*mulib_csv_row_callback) (void *user, const char *const *fields,
                                         size_t count);
  /** @brief mulib csv parse; see manual/mulib-csv.md for contracts and
   * examples. */
  int mulib_csv_parse (const char *source, mulib_csv_row_callback callback,
                       void *user);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSV_H */

#if defined(MULIB_IMPL_CSV) && !defined(MULIB_CSV_IMPLEMENTED)
#define MULIB_CSV_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_csv_parse (const char *s, mulib_csv_row_callback cb, void *user)
  {
    size_t cap = 8, n = 0;
    char **fields;
    int ok = 0;
    if (!s || !cb)
      return 0;
    fields = (char **)malloc (cap * sizeof (*fields));
    if (!fields)
      return 0;
    while (*s)
      {
        int done = 0;
        while (!done)
          {
            size_t capacity = 64, length = 0;
            char *b = (char *)malloc (capacity);
            int quoted = *s == '"', closed = !quoted;
            if (!b)
              goto end;
            if (quoted)
              ++s;
            while (*s)
              {
                if (quoted && *s == '"')
                  {
                    if (s[1] == '"')
                      ++s;
                    else
                      {
                        ++s;
                        closed = 1;
                        break;
                      }
                  }
                else if (!quoted)
                  {
                    if (*s == ',' || *s == '\r' || *s == '\n')
                      break;
                    if (*s == '"')
                      {
                        free (b);
                        goto end;
                      }
                  }
                if (length == capacity - 1)
                  {
                    size_t next_capacity;
                    char *next;
                    if (capacity > SIZE_MAX / 2)
                      {
                        free (b);
                        goto end;
                      }
                    next_capacity = capacity * 2;
                    next = (char *)realloc (b, next_capacity);
                    if (!next)
                      {
                        free (b);
                        goto end;
                      }
                    b = next;
                    capacity = next_capacity;
                  }
                b[length++] = *s++;
              }
            if (!closed || (*s && *s != ',' && *s != '\r' && *s != '\n'))
              {
                free (b);
                goto end;
              }
            b[length] = 0;
            if (n == cap)
              {
                char **next;
                if (cap > SIZE_MAX / 2 / sizeof (*fields))
                  {
                    free (b);
                    goto end;
                  }
                next = (char **)realloc (fields, cap * 2 * sizeof (*fields));
                if (!next)
                  {
                    free (b);
                    goto end;
                  }
                fields = next;
                cap *= 2;
              }
            fields[n++] = b;
            if (*s == ',')
              {
                ++s;
                continue;
              }
            if (*s == '\r')
              ++s;
            if (*s == '\n')
              ++s;
            done = 1;
          }
        if (!cb (user, (const char *const *)fields, n))
          goto end;
        for (size_t i = 0; i < n; ++i)
          free (fields[i]);
        n = 0;
      }
    ok = 1;
  end:
    for (size_t i = 0; i < n; ++i)
      free (fields[i]);
    free (fields);
    return ok;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
