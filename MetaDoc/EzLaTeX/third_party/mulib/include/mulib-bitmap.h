/**
 * @file mulib-bitmap.h
 * @brief Packed one-bit raster image with per-pixel access.
 * @defgroup mulib_bitmap MuLIB bitmap
 * @{
 * @details Useful for monochrome masks and occupancy maps. Coordinates are
 * zero-based; invalid reads return zero and invalid writes fail.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_BITMAP before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_BITMAP_H
#define MULIB_BITMAP_H
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

  /** @brief Packed one-bit raster image with per-pixel access. */
  typedef struct mulib_bitmap
  {
    unsigned char *pixels;
    size_t width, height;
  } mulib_bitmap_t;
  /** @brief mulib bitmap init; see manual/mulib-bitmap.md for contracts and
   * examples. */
  int mulib_bitmap_init (mulib_bitmap_t *b, size_t width, size_t height);
  /** @brief mulib bitmap get; see manual/mulib-bitmap.md for contracts and
   * examples. */
  int mulib_bitmap_get (const mulib_bitmap_t *b, size_t x, size_t y);
  /** @brief mulib bitmap put; see manual/mulib-bitmap.md for contracts and
   * examples. */
  int mulib_bitmap_put (mulib_bitmap_t *b, size_t x, size_t y, int on);
  /** @brief mulib bitmap free; see manual/mulib-bitmap.md for contracts and
   * examples. */
  void mulib_bitmap_free (mulib_bitmap_t *b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_BITMAP_H */

#if defined(MULIB_IMPL_BITMAP) && !defined(MULIB_BITMAP_IMPLEMENTED)
#define MULIB_BITMAP_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_bitmap_init (mulib_bitmap_t *b, size_t w, size_t h)
  {
    if (!b || (w && h > SIZE_MAX / w) || w * h > SIZE_MAX - 7)
      return 0;
    unsigned char *p = calloc (w && h ? (w * h + 7) / 8 : 1, 1);
    if (!p)
      return 0;
    b->pixels = p;
    b->width = w;
    b->height = h;
    return 1;
  }
  int
  mulib_bitmap_get (const mulib_bitmap_t *b, size_t x, size_t y)
  {
    if (!b || !b->pixels || x >= b->width || y >= b->height)
      return 0;
    size_t i = y * b->width + x;
    return (b->pixels[i / 8] >> (i % 8)) & 1;
  }
  int
  mulib_bitmap_put (mulib_bitmap_t *b, size_t x, size_t y, int on)
  {
    if (!b || !b->pixels || x >= b->width || y >= b->height)
      return 0;
    size_t i = y * b->width + x;
    unsigned char mask = (unsigned char)(1u << (i % 8));
    if (on)
      b->pixels[i / 8] |= mask;
    else
      b->pixels[i / 8] &= (unsigned char)~mask;
    return 1;
  }
  void
  mulib_bitmap_free (mulib_bitmap_t *b)
  {
    if (b)
      {
        free (b->pixels);
        memset (b, 0, sizeof (*b));
      }
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
