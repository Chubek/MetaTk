/**
 * @file mulib-keyremap.h
 * @brief Stateful key-code remapping with balanced many-to-one key releases.
 * Define MULIB_IMPL_KEYREMAP and MULIB_IMPL_INPUT once.
 */
#ifndef MULIB_KEYREMAP_H
#define MULIB_KEYREMAP_H
#include "mulib-input.h"
#ifdef __cplusplus
extern "C"
{
#endif
#define MULIB_KEYREMAP_DROP UINT16_MAX
  /** @brief Value-owned state; initialize before use. Changes apply at next
   * key press. */
  typedef struct mulib_keyremap
  {
    uint16_t mapping[MULIB_INPUT_KEY_COUNT], active[MULIB_INPUT_KEY_COUNT],
        references[MULIB_INPUT_KEY_COUNT];
    unsigned char pressed[MULIB_INPUT_KEY_COUNT];
  } mulib_keyremap_t;
  /** @brief Identity mapping and no held keys; resetting does not emit
   * releases. */
  void mulib_keyremap_init (mulib_keyremap_t *remap);
  /** @brief Map a key to another key or DROP; return 1 on success, 0 if
   * invalid. */
  int mulib_keyremap_set (mulib_keyremap_t *remap, unsigned source,
                          unsigned target);
  /** @brief Consume one event: 1 output, 0 suppressed, -1 invalid (no
   * mutation). Non-key events pass through. Duplicate presses and orphan
   * releases/repeats are suppressed. Output may alias input. SYN_DROPPED
   * requires caller recovery.
   */
  int mulib_keyremap_apply (mulib_keyremap_t *remap,
                            const mulib_input_event_t *event,
                            mulib_input_event_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_KEYREMAP) && !defined(MULIB_KEYREMAP_IMPLEMENTED)
#define MULIB_KEYREMAP_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_keyremap_init (mulib_keyremap_t *r)
  {
    if (!r)
      return;
    memset (r, 0, sizeof (*r));
    for (unsigned i = 0; i < MULIB_INPUT_KEY_COUNT; ++i)
      r->mapping[i] = (uint16_t)i;
  }
  int
  mulib_keyremap_set (mulib_keyremap_t *r, unsigned from, unsigned to)
  {
    if (!r || from >= MULIB_INPUT_KEY_COUNT
        || (to >= MULIB_INPUT_KEY_COUNT && to != MULIB_KEYREMAP_DROP))
      return 0;
    r->mapping[from] = (uint16_t)to;
    return 1;
  }
  int
  mulib_keyremap_apply (mulib_keyremap_t *r, const mulib_input_event_t *e,
                        mulib_input_event_t *out)
  {
    unsigned key, target;
    mulib_input_event_t value;
    if (!r || !out || !mulib_input_event_valid (e))
      return -1;
    value = *e;
    if (e->type != MULIB_INPUT_KEY)
      {
        *out = value;
        return 1;
      }
    key = e->code;
    if (e->value == 1)
      {
        if (r->pressed[key])
          return 0;
        r->pressed[key] = 1;
        target = r->active[key] = r->mapping[key];
        if (target == MULIB_KEYREMAP_DROP || r->references[target]++)
          return 0;
      }
    else
      {
        if (!r->pressed[key])
          return 0;
        target = r->active[key];
        if (!e->value)
          {
            r->pressed[key] = 0;
            if (target == MULIB_KEYREMAP_DROP || --r->references[target])
              return 0;
          }
        else if (target == MULIB_KEYREMAP_DROP)
          return 0;
      }
    value.code = (uint16_t)target;
    *out = value;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
