/**
 * @file mulib-wlroots.h
 * @brief Dependency-free wlroots-style output boxes, transforms and layout
 * bookkeeping. Not a compositor or wlroots ABI binding. Define WLROOTS and VEC
 * switches once.
 */
#ifndef MULIB_WLROOTS_H
#define MULIB_WLROOTS_H
#include "mulib-vec.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Half-open integer box. Nonnegative sizes; endpoints must fit
   * int32_t. */
  typedef struct mulib_wlroots_box
  {
    int32_t x, y, width, height;
  } mulib_wlroots_box_t;
  /** @brief Values match wl_output_transform, including flipped rotations. */
  enum mulib_wlroots_transform
  {
    MULIB_WLROOTS_NORMAL,
    MULIB_WLROOTS_90,
    MULIB_WLROOTS_180,
    MULIB_WLROOTS_270,
    MULIB_WLROOTS_FLIPPED,
    MULIB_WLROOTS_FLIPPED_90,
    MULIB_WLROOTS_FLIPPED_180,
    MULIB_WLROOTS_FLIPPED_270
  };
  /** @brief Return 1 for valid boxes (including empty), else 0. */
  int mulib_wlroots_box_valid (const mulib_wlroots_box_t *box);
  /** @brief Test half-open containment; empty/invalid boxes return 0. */
  int mulib_wlroots_box_contains (const mulib_wlroots_box_t *box, int32_t x,
                                  int32_t y);
  /** @brief Return 1 and intersection; empty/invalid intersections leave
   * output unchanged. */
  int mulib_wlroots_box_intersection (const mulib_wlroots_box_t *a,
                                      const mulib_wlroots_box_t *b,
                                      mulib_wlroots_box_t *out);
  /** @brief Transform a box inside a nonnegative width/height canvas; return 1
   * or 0. Output may alias input; unchanged on invalid/out-of-canvas input.
   */
  int mulib_wlroots_box_transform (const mulib_wlroots_box_t *box,
                                   unsigned transform, int32_t width,
                                   int32_t height, mulib_wlroots_box_t *out);
  /** @brief Copyable caller-assigned output identity and logical box. */
  typedef struct mulib_wlroots_output
  {
    uint32_t id;
    mulib_wlroots_box_t box;
  } mulib_wlroots_output_t;
  /** @brief Owned layout; initialize and do not copy. No native output
   * pointers retained. */
  typedef struct mulib_wlroots_layout
  {
    mulib_vec_t outputs;
  } mulib_wlroots_layout_t;
  /** @brief Initialize empty layout. */
  void mulib_wlroots_layout_init (mulib_wlroots_layout_t *layout);
  /** @brief Free/reset layout, NULL safe. */
  void mulib_wlroots_layout_free (mulib_wlroots_layout_t *layout);
  /** @brief Add/update nonempty output with nonzero id; 1 success, 0
   * invalid/OOM. */
  int mulib_wlroots_layout_set (mulib_wlroots_layout_t *layout, uint32_t id,
                                const mulib_wlroots_box_t *box);
  /** @brief Remove id; 1 removed, 0 absent/invalid. */
  int mulib_wlroots_layout_remove (mulib_wlroots_layout_t *layout,
                                   uint32_t id);
  /** @brief Return first-added overlapping output id at point, or zero. */
  uint32_t mulib_wlroots_layout_at (const mulib_wlroots_layout_t *layout,
                                    int32_t x, int32_t y);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WLROOTS) && !defined(MULIB_WLROOTS_IMPLEMENTED)
#define MULIB_WLROOTS_IMPLEMENTED
#include <limits.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_wlroots_box_valid (const mulib_wlroots_box_t *b)
  {
    return b && b->width >= 0 && b->height >= 0
           && (int64_t)b->x + b->width <= INT32_MAX
           && (int64_t)b->y + b->height <= INT32_MAX;
  }
  int
  mulib_wlroots_box_contains (const mulib_wlroots_box_t *b, int32_t x,
                              int32_t y)
  {
    return mulib_wlroots_box_valid (b) && x >= b->x && y >= b->y
           && (int64_t)x - b->x < b->width && (int64_t)y - b->y < b->height;
  }
  int
  mulib_wlroots_box_intersection (const mulib_wlroots_box_t *a,
                                  const mulib_wlroots_box_t *b,
                                  mulib_wlroots_box_t *out)
  {
    int64_t x, y, right, bottom;
    mulib_wlroots_box_t value;
    if (!out || !mulib_wlroots_box_valid (a) || !mulib_wlroots_box_valid (b))
      return 0;
    x = a->x > b->x ? a->x : b->x;
    y = a->y > b->y ? a->y : b->y;
    right = (int64_t)a->x + a->width < (int64_t)b->x + b->width
                ? (int64_t)a->x + a->width
                : (int64_t)b->x + b->width;
    bottom = (int64_t)a->y + a->height < (int64_t)b->y + b->height
                 ? (int64_t)a->y + a->height
                 : (int64_t)b->y + b->height;
    if (right <= x || bottom <= y)
      return 0;
    value.x = (int32_t)x;
    value.y = (int32_t)y;
    value.width = (int32_t)(right - x);
    value.height = (int32_t)(bottom - y);
    *out = value;
    return 1;
  }
  int
  mulib_wlroots_box_transform (const mulib_wlroots_box_t *b, unsigned t,
                               int32_t w, int32_t h, mulib_wlroots_box_t *out)
  {
    mulib_wlroots_box_t v;
    if (!out || !mulib_wlroots_box_valid (b) || t > 7 || w < 0 || h < 0
        || b->x < 0 || b->y < 0 || (int64_t)b->x + b->width > w
        || (int64_t)b->y + b->height > h)
      return 0;
    v = *b;
    switch (t)
      {
      case 1:
        v.x = h - b->y - b->height;
        v.y = b->x;
        break;
      case 2:
        v.x = w - b->x - b->width;
        v.y = h - b->y - b->height;
        break;
      case 3:
        v.x = b->y;
        v.y = w - b->x - b->width;
        break;
      case 4:
        v.x = w - b->x - b->width;
        break;
      case 5:
        v.x = h - b->y - b->height;
        v.y = w - b->x - b->width;
        break;
      case 6:
        v.y = h - b->y - b->height;
        break;
      case 7:
        v.x = b->y;
        v.y = b->x;
        break;
      default:
        break;
      }
    if (t & 1)
      {
        v.width = b->height;
        v.height = b->width;
      }
    *out = v;
    return 1;
  }
  void
  mulib_wlroots_layout_init (mulib_wlroots_layout_t *l)
  {
    if (l)
      mulib_vec_init (&l->outputs, sizeof (mulib_wlroots_output_t));
  }
  void
  mulib_wlroots_layout_free (mulib_wlroots_layout_t *l)
  {
    if (l)
      mulib_vec_free (&l->outputs);
  }
  int
  mulib_wlroots_layout_set (mulib_wlroots_layout_t *l, uint32_t id,
                            const mulib_wlroots_box_t *b)
  {
    mulib_wlroots_output_t value;
    if (!l || !id || !mulib_wlroots_box_valid (b) || !b->width || !b->height)
      return 0;
    value.id = id;
    value.box = *b;
    for (size_t i = 0; i < l->outputs.length; ++i)
      {
        mulib_wlroots_output_t *p
            = (mulib_wlroots_output_t *)mulib_vec_at (&l->outputs, i);
        if (p->id == id)
          {
            *p = value;
            return 1;
          }
      }
    return mulib_vec_push (&l->outputs, &value);
  }
  int
  mulib_wlroots_layout_remove (mulib_wlroots_layout_t *l, uint32_t id)
  {
    if (!l)
      return 0;
    for (size_t i = 0; i < l->outputs.length; ++i)
      {
        mulib_wlroots_output_t *p
            = (mulib_wlroots_output_t *)mulib_vec_at (&l->outputs, i);
        if (p->id == id)
          {
            memmove (p, p + 1, (l->outputs.length - i - 1) * sizeof (*p));
            --l->outputs.length;
            return 1;
          }
      }
    return 0;
  }
  uint32_t
  mulib_wlroots_layout_at (const mulib_wlroots_layout_t *l, int32_t x,
                           int32_t y)
  {
    if (!l)
      return 0;
    for (size_t i = 0; i < l->outputs.length; ++i)
      {
        const mulib_wlroots_output_t *p
            = (const mulib_wlroots_output_t *)(l->outputs.data
                                               + i * sizeof (*p));
        if (mulib_wlroots_box_contains (&p->box, x, y))
          return p->id;
      }
    return 0;
  }
#ifdef __cplusplus
}
#endif
#endif
