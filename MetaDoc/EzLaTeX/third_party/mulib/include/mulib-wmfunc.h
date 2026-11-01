/** @file mulib-wmfunc.h
 * @brief Portable window placement and transactional backend action dispatch.
 * Define MULIB_IMPL_WMFUNC, WLROOTS and VEC explicitly once.
 */
#ifndef MULIB_WMFUNC_H
#define MULIB_WMFUNC_H
#include "mulib-wlroots.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Return API revision 1. */
  unsigned mulib_wmfunc_version (void);
  typedef enum mulib_wmfunc_action
  {
    MULIB_WMFUNC_PLACE,
    MULIB_WMFUNC_MAXIMIZE,
    MULIB_WMFUNC_RESTORE,
    MULIB_WMFUNC_FOCUS,
    MULIB_WMFUNC_CLOSE
  } mulib_wmfunc_action_t;
  /** @brief Application-owned window state; id is nonzero, boxes are nonempty.
   */
  typedef struct mulib_wmfunc_window
  {
    uint32_t id;
    mulib_wlroots_box_t box, restore;
    int maximized;
  } mulib_wmfunc_window_t;
  /** @brief Backend borrows candidate state during callback. Return 1 on
   * accepted action, 0 on failure. Do not reenter/mutate this window. No
   * callback is retained.
   */
  typedef int (*mulib_wmfunc_backend_fn) (
      void *user, mulib_wmfunc_action_t action,
      const mulib_wmfunc_window_t *candidate);
  /** @brief Initialize window; output unchanged on invalid id/box. */
  int mulib_wmfunc_init (mulib_wmfunc_window_t *window, uint32_t id,
                         const mulib_wlroots_box_t *box);
  /** @brief Fit a window into a work area, shrinking if necessary. Allows
   * aliasing; output unchanged on invalid/empty boxes. Uses half-open
   * coordinates.
   */
  int mulib_wmfunc_fit (const mulib_wlroots_box_t *box,
                        const mulib_wlroots_box_t *area,
                        mulib_wlroots_box_t *out);
  /** @brief Validate and dispatch; commit state only when backend returns 1.
   * PLACE takes geometry; MAXIMIZE takes work area; others ignore geometry.
   * Repeated maximize preserves restore geometry. Restore when normal is a
   * no-op. Focus/close only dispatch requests; no native handles are owned or
   * destroyed.
   */
  int mulib_wmfunc_apply (mulib_wmfunc_window_t *window,
                          mulib_wmfunc_action_t action,
                          const mulib_wlroots_box_t *geometry,
                          mulib_wmfunc_backend_fn backend, void *user);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WMFUNC) && !defined(MULIB_WMFUNC_IMPLEMENTED)
#define MULIB_WMFUNC_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  unsigned
  mulib_wmfunc_version (void)
  {
    return 1;
  }
  static int
  mulib_wmfunc_box_ (const mulib_wlroots_box_t *b)
  {
    return mulib_wlroots_box_valid (b) && b->width > 0 && b->height > 0;
  }
  int
  mulib_wmfunc_init (mulib_wmfunc_window_t *w, uint32_t id,
                     const mulib_wlroots_box_t *b)
  {
    if (!w || !id || !mulib_wmfunc_box_ (b))
      return 0;
    w->id = id;
    w->box = *b;
    w->restore = *b;
    w->maximized = 0;
    return 1;
  }
  int
  mulib_wmfunc_fit (const mulib_wlroots_box_t *b, const mulib_wlroots_box_t *a,
                    mulib_wlroots_box_t *out)
  {
    mulib_wlroots_box_t result;
    if (!out || !mulib_wmfunc_box_ (b) || !mulib_wmfunc_box_ (a))
      return 0;
    result = *b;
    if (result.width > a->width)
      result.width = a->width;
    if (result.height > a->height)
      result.height = a->height;
    if (result.x < a->x)
      result.x = a->x;
    if (result.y < a->y)
      result.y = a->y;
    if ((int64_t)result.x + result.width > (int64_t)a->x + a->width)
      result.x = (int32_t)((int64_t)a->x + a->width - result.width);
    if ((int64_t)result.y + result.height > (int64_t)a->y + a->height)
      result.y = (int32_t)((int64_t)a->y + a->height - result.height);
    *out = result;
    return 1;
  }
  int
  mulib_wmfunc_apply (mulib_wmfunc_window_t *w, mulib_wmfunc_action_t action,
                      const mulib_wlroots_box_t *geometry,
                      mulib_wmfunc_backend_fn backend, void *user)
  {
    mulib_wmfunc_window_t candidate;
    if (!w || !w->id || !backend || !mulib_wmfunc_box_ (&w->box)
        || !mulib_wmfunc_box_ (&w->restore)
        || (w->maximized != 0 && w->maximized != 1))
      return 0;
    candidate = *w;
    switch (action)
      {
      case MULIB_WMFUNC_PLACE:
        if (!mulib_wmfunc_box_ (geometry))
          return 0;
        candidate.box = *geometry;
        candidate.restore = *geometry;
        candidate.maximized = 0;
        break;
      case MULIB_WMFUNC_MAXIMIZE:
        if (!mulib_wmfunc_box_ (geometry))
          return 0;
        if (!candidate.maximized)
          candidate.restore = candidate.box;
        candidate.box = *geometry;
        candidate.maximized = 1;
        break;
      case MULIB_WMFUNC_RESTORE:
        if (!candidate.maximized)
          return 1;
        candidate.box = candidate.restore;
        candidate.maximized = 0;
        break;
      case MULIB_WMFUNC_FOCUS:
      case MULIB_WMFUNC_CLOSE:
        break;
      default:
        return 0;
      }
    if (backend (user, action, &candidate) != 1)
      return 0;
    *w = candidate;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
