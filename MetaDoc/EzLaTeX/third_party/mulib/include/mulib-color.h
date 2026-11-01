/**
 * @file mulib-color.h
 * @brief RGB to HSV and HSV to RGB color-model conversion.
 * @defgroup mulib_color MuLIB color
 * @{
 * @details RGB channels are assumed to lie between 0 and 1 and HSV hue is
 * measured in degrees. The API does not apply gamma correction or clamp
 * invalid input.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_COLOR before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_COLOR_H
#define MULIB_COLOR_H
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

  /** @brief RGB to HSV and HSV to RGB color-model conversion. */
  typedef struct mulib_color_rgb
  {
    double r, g, b;
  } mulib_color_rgb_t;
  /** @brief RGB to HSV and HSV to RGB color-model conversion. */
  typedef struct mulib_color_hsv
  {
    double h, s, v;
  } mulib_color_hsv_t;
  /** @brief mulib color to hsv; see manual/mulib-color.md for contracts and
   * examples. */
  mulib_color_hsv_t mulib_color_to_hsv (mulib_color_rgb_t color);
  /** @brief mulib color from hsv; see manual/mulib-color.md for contracts and
   * examples. */
  mulib_color_rgb_t mulib_color_from_hsv (mulib_color_hsv_t hsv);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_COLOR_H */

#if defined(MULIB_IMPL_COLOR) && !defined(MULIB_COLOR_IMPLEMENTED)
#define MULIB_COLOR_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_color_hsv_t
  mulib_color_to_hsv (mulib_color_rgb_t c)
  {
    double max = fmax (c.r, fmax (c.g, c.b)),
           min = fmin (c.r, fmin (c.g, c.b)), d = max - min;
    mulib_color_hsv_t h = { 0, max ? d / max : 0, max };
    if (d > 0)
      {
        if (max == c.r)
          h.h = fmod ((c.g - c.b) / d, 6);
        else if (max == c.g)
          h.h = (c.b - c.r) / d + 2;
        else
          h.h = (c.r - c.g) / d + 4;
        h.h *= 60;
        if (h.h < 0)
          h.h += 360;
      }
    return h;
  }
  mulib_color_rgb_t
  mulib_color_from_hsv (mulib_color_hsv_t h)
  {
    double angle = fmod (h.h, 360);
    if (angle < 0)
      angle += 360;
    double chroma = h.v * h.s,
           x = chroma * (1 - fabs (fmod (angle / 60, 2) - 1)),
           m = h.v - chroma;
    double r = 0, g = 0, b = 0;
    if (angle < 60)
      {
        r = chroma;
        g = x;
      }
    else if (angle < 120)
      {
        r = x;
        g = chroma;
      }
    else if (angle < 180)
      {
        g = chroma;
        b = x;
      }
    else if (angle < 240)
      {
        g = x;
        b = chroma;
      }
    else if (angle < 300)
      {
        r = x;
        b = chroma;
      }
    else
      {
        r = chroma;
        b = x;
      }
    return (mulib_color_rgb_t){ r + m, g + m, b + m };
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
