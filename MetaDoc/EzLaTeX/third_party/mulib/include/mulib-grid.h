/**
 * @file mulib-grid.h
 * @brief Two-dimensional packed grid of uniform-sized cells.
 * @defgroup mulib_grid MuLIB grid
 * @{
 * @details Cells are zero-filled initially and laid out in row-major order.
 * The grid owns bytes, not individual object resources.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_GRID before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_GRID_H
#define MULIB_GRID_H
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

  /** @brief Two-dimensional packed grid of uniform-sized cells. */
  typedef struct mulib_grid
  {
    unsigned char *data;
    size_t width, height, stride;
  } mulib_grid_t;
  /** @brief mulib grid init; see manual/mulib-grid.md for contracts and
   * examples. */
  int mulib_grid_init (mulib_grid_t *g, size_t width, size_t height,
                       size_t stride);
  /** @brief mulib grid at; see manual/mulib-grid.md for contracts and
   * examples. */
  void *mulib_grid_at (mulib_grid_t *g, size_t x, size_t y);
  /** @brief mulib grid free; see manual/mulib-grid.md for contracts and
   * examples. */
  void mulib_grid_free (mulib_grid_t *g);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_GRID_H */

#if defined(MULIB_IMPL_GRID) && !defined(MULIB_GRID_IMPLEMENTED)
#define MULIB_GRID_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_grid_init (mulib_grid_t *g, size_t w, size_t h, size_t s)
  {
    if (!g || !s || (w && h > SIZE_MAX / w) || w * h > SIZE_MAX / s)
      return 0;
    unsigned char *p = calloc (w && h ? w * h : 1, s);
    if (!p)
      return 0;
    g->data = p;
    g->width = w;
    g->height = h;
    g->stride = s;
    return 1;
  }
  void *
  mulib_grid_at (mulib_grid_t *g, size_t x, size_t y)
  {
    return g && g->data && x < g->width && y < g->height
               ? g->data + (y * g->width + x) * g->stride
               : NULL;
  }
  void
  mulib_grid_free (mulib_grid_t *g)
  {
    if (g)
      {
        free (g->data);
        memset (g, 0, sizeof (*g));
      }
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
