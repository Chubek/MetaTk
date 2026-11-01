/**
 * @file mulib-hex.h
 * @brief Lowercase hexadecimal encoding and case-insensitive decoding.
 * @defgroup mulib_hex MuLIB hex
 * @{
 * @details Output buffers must have capacity for every encoded or decoded
 * byte. Decoder does not accept prefixes such as 0x or whitespace.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_HEX before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_HEX_H
#define MULIB_HEX_H
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

  /** @brief mulib hex encode; see manual/mulib-hex.md for contracts and
   * examples. */
  size_t mulib_hex_encode (const void *data, size_t n, char *out, size_t cap);
  /** @brief Decode even-length hex, leaving output unchanged on error; exact
   * in-place is safe. */
  int mulib_hex_decode (const char *source, size_t n, unsigned char *out,
                        size_t cap);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_HEX_H */

#if defined(MULIB_IMPL_HEX) && !defined(MULIB_HEX_IMPLEMENTED)
#define MULIB_HEX_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_hex_encode (const void *p, size_t n, char *out, size_t cap)
  {
    static const char x[] = "0123456789abcdef";
    if ((!p && n) || n > (SIZE_MAX - 1) / 2 || !out || cap < 2 * n + 1)
      return 0;
    const unsigned char *b = p;
    for (size_t i = 0; i < n; i++)
      {
        out[i * 2] = x[b[i] >> 4];
        out[i * 2 + 1] = x[b[i] & 15];
      }
    out[n * 2] = 0;
    return n * 2;
  }
  static int
  mulib_hex_value_ (char c)
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
  mulib_hex_decode (const char *s, size_t n, unsigned char *out, size_t cap)
  {
    if ((!s && n) || n % 2 || n / 2 > cap || (!out && n))
      return 0;
    for (size_t i = 0; i < n; i++)
      if (mulib_hex_value_ (s[i]) < 0)
        return 0;
    for (size_t i = 0; i < n / 2; i++)
      {
        int a = mulib_hex_value_ (s[i * 2]),
            b = mulib_hex_value_ (s[i * 2 + 1]);
        out[i] = (unsigned char)((a << 4) | b);
      }
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
