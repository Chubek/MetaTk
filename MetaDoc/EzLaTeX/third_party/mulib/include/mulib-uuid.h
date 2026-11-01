/**
 * @file mulib-uuid.h
 * @brief Formatting and parsing RFC 4122-style 128-bit UUID text.
 * @defgroup mulib_uuid MuLIB uuid
 * @{
 * @details Parses and formats standard UUID hyphen groups. Does not generate
 * UUIDs or validate version/variant fields; use OS random APIs for
 * cryptographically secure generation.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_UUID before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_UUID_H
#define MULIB_UUID_H
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

  /** @brief Formatting and parsing RFC 4122-style 128-bit UUID text. */
  typedef struct mulib_uuid
  {
    unsigned char bytes[16];
  } mulib_uuid_t;
  /** @brief mulib uuid parse; see manual/mulib-uuid.md for contracts and
   * examples. */
  int mulib_uuid_parse (const char *text, mulib_uuid_t *out);
  /** @brief mulib uuid format; see manual/mulib-uuid.md for contracts and
   * examples. */
  void mulib_uuid_format (const mulib_uuid_t *id, char out[37]);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_UUID_H */

#if defined(MULIB_IMPL_UUID) && !defined(MULIB_UUID_IMPLEMENTED)
#define MULIB_UUID_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_uuid_digit_ (char c)
  {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  }
  int
  mulib_uuid_parse (const char *s, mulib_uuid_t *out)
  {
    if (!s || !out || strlen (s) != 36)
      return 0;
    unsigned char b[16];
    size_t pos = 0;
    for (size_t i = 0; i < 36;)
      {
        if (i == 8 || i == 13 || i == 18 || i == 23)
          {
            if (s[i++] != '-')
              return 0;
            continue;
          }
        int x = mulib_uuid_digit_ (s[i++]), y = mulib_uuid_digit_ (s[i++]);
        if (x < 0 || y < 0 || pos >= 16)
          return 0;
        b[pos++] = (unsigned char)((x << 4) | y);
      }
    memcpy (out->bytes, b, 16);
    return 1;
  }
  void
  mulib_uuid_format (const mulib_uuid_t *id, char out[37])
  {
    static const char a[] = "0123456789abcdef";
    if (!id || !out)
      return;
    size_t j = 0;
    for (size_t i = 0; i < 16; i++)
      {
        if (i == 4 || i == 6 || i == 8 || i == 10)
          out[j++] = '-';
        out[j++] = a[id->bytes[i] >> 4];
        out[j++] = a[id->bytes[i] & 15];
      }
    out[j] = 0;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
