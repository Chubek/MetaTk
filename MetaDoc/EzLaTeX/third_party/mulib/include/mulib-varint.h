/**
 * @file mulib-varint.h
 * @brief Unsigned LEB128 integer encoding and bounds-checked decoding.
 * @defgroup mulib_varint MuLIB varint
 * @{
 * @details Rejects overflow, truncation, and unterminated input. Non-minimal
 * encodings are accepted and decode to the same unsigned integer.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_VARINT before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_VARINT_H
#define MULIB_VARINT_H
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

  /** @brief mulib varint encode; see manual/mulib-varint.md for contracts and
   * examples. */
  size_t mulib_varint_encode (uint64_t value, unsigned char out[10]);
  /** @brief mulib varint decode; see manual/mulib-varint.md for contracts and
   * examples. */
  int mulib_varint_decode (const unsigned char *data, size_t length,
                           uint64_t *out, size_t *consumed);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_VARINT_H */

#if defined(MULIB_IMPL_VARINT) && !defined(MULIB_VARINT_IMPLEMENTED)
#define MULIB_VARINT_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_varint_encode (uint64_t n, unsigned char out[10])
  {
    if (!out)
      return 0;
    size_t i = 0;
    do
      {
        unsigned char b = (unsigned char)(n & 0x7f);
        n >>= 7;
        out[i++] = (unsigned char)(b | (n ? 0x80 : 0));
      }
    while (n);
    return i;
  }
  int
  mulib_varint_decode (const unsigned char *p, size_t n, uint64_t *out,
                       size_t *used)
  {
    if (!p || !out)
      return 0;
    uint64_t value = 0;
    for (size_t i = 0; i < n && i < 10; i++)
      {
        unsigned char b = p[i];
        if (i == 9 && (b & 0xfe))
          return 0;
        value |= ((uint64_t)(b & 127)) << (i * 7);
        if (!(b & 128))
          {
            *out = value;
            if (used)
              *used = i + 1;
            return 1;
          }
      }
    return 0;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
