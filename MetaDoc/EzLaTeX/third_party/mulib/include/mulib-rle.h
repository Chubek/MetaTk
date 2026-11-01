/**
 * @file mulib-rle.h
 * @brief Byte run-length encoding and bounded decoding.
 * @defgroup mulib_rle MuLIB rle
 * @{
 * @details Wire format repeats pairs `(count, byte)` where counts
 * range 1..255. No checksum or length header is included; compressed data can
 * be larger than source.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_RLE before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_RLE_H
#define MULIB_RLE_H
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

  /** @brief mulib rle encode; see manual/mulib-rle.md for contracts and
   * examples. */
  size_t mulib_rle_encode (const unsigned char *in, size_t n,
                           unsigned char *out, size_t cap);
  /** @brief Decode disjoint buffers, leaving output/written unchanged on
   * malformed data or insufficient capacity. */
  int mulib_rle_decode (const unsigned char *in, size_t n, unsigned char *out,
                        size_t cap, size_t *written);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_RLE_H */

#if defined(MULIB_IMPL_RLE) && !defined(MULIB_RLE_IMPLEMENTED)
#define MULIB_RLE_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_rle_encode (const unsigned char *s, size_t n, unsigned char *out,
                    size_t cap)
  {
    if ((!s && n) || !out)
      return 0;
    size_t j = 0;
    for (size_t i = 0; i < n;)
      {
        size_t run = 1;
        while (run < n - i && run < 255 && s[i + run] == s[i])
          run++;
        if (j > cap || cap - j < 2)
          return 0;
        out[j++] = (unsigned char)run;
        out[j++] = s[i];
        i += run;
      }
    return j;
  }
  int
  mulib_rle_decode (const unsigned char *s, size_t n, unsigned char *out,
                    size_t cap, size_t *written)
  {
    if ((!s && n) || (!out && n) || n % 2)
      return 0;
    size_t size = 0, j = 0;
    for (size_t i = 0; i < n; i += 2)
      {
        size_t run = s[i];
        if (!run || run > cap - size)
          return 0;
        size += run;
      }
    for (size_t i = 0; i < n; i += 2)
      {
        size_t run = s[i];
        memset (out + j, s[i + 1], run);
        j += run;
      }
    if (written)
      *written = size;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
