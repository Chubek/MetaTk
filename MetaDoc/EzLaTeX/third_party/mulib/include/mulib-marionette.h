/**
 * @file mulib-marionette.h
 * @brief Deterministic, caller-clocked input recording and callback playback.
 * Define MULIB_IMPL_MARIONETTE, MULIB_IMPL_INPUT and MULIB_IMPL_VEC once.
 */
#ifndef MULIB_MARIONETTE_H
#define MULIB_MARIONETTE_H
#include "mulib-input.h"
#include "mulib-vec.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Sink returns zero after accepting an event; failure leaves it
   * pending. */
  typedef int (*mulib_marionette_sink_fn) (const mulib_input_event_t *event,
                                           void *user);
  /** @brief Owned event vector and playback cursor; initialize, do not copy.
   */
  typedef struct mulib_marionette
  {
    mulib_vec_t events;
    size_t cursor, limit;
    uint64_t start;
    int playing;
  } mulib_marionette_t;
  /** @brief Limit zero selects 1,000,000 events. Event times are offsets from
   * playback start. */
  void mulib_marionette_init (mulib_marionette_t *recording, size_t limit);
  /** @brief Free and reset; NULL safe. */
  void mulib_marionette_free (mulib_marionette_t *recording);
  /** @brief Append a valid, nondecreasing timestamp; 1 success, 0
   * invalid/full/OOM/playing. */
  int mulib_marionette_append (mulib_marionette_t *recording,
                               const mulib_input_event_t *event);
  /** @brief Restart at caller's start time; 1 success, 0 timestamp
   * overflow/invalid. */
  int mulib_marionette_start (mulib_marionette_t *recording,
                              uint64_t start_us);
  /** @brief Stop without emitting releases or clearing the recording. */
  void mulib_marionette_stop (mulib_marionette_t *recording);
  /** @brief Emit due events, at most budget (zero = unlimited); 0 success, -1
   * invalid, -2 sink failure. sent is set even after partial success. No
   * sleeps or native injection. Sink must not mutate/reenter this recording.
   * Backward clocks emit nothing early.
   */
  int mulib_marionette_tick (mulib_marionette_t *recording, uint64_t now_us,
                             size_t budget, mulib_marionette_sink_fn sink,
                             void *user, size_t *sent);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MARIONETTE) && !defined(MULIB_MARIONETTE_IMPLEMENTED)
#define MULIB_MARIONETTE_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_marionette_init (mulib_marionette_t *r, size_t limit)
  {
    if (!r)
      return;
    mulib_vec_init (&r->events, sizeof (mulib_input_event_t));
    r->cursor = 0;
    r->limit = limit ? limit : 1000000;
    r->start = 0;
    r->playing = 0;
  }
  void
  mulib_marionette_free (mulib_marionette_t *r)
  {
    if (!r)
      return;
    mulib_vec_free (&r->events);
    mulib_marionette_init (r, r->limit);
  }
  int
  mulib_marionette_append (mulib_marionette_t *r, const mulib_input_event_t *e)
  {
    const mulib_input_event_t *last;
    mulib_input_event_t value;
    if (!r || !mulib_input_event_valid (e) || r->playing
        || r->events.length >= r->limit)
      return 0;
    last = (const mulib_input_event_t *)mulib_vec_at (&r->events,
                                                      r->events.length - 1);
    if (last && last->time_us > e->time_us)
      return 0;
    value = *e;
    return mulib_vec_push (&r->events, &value);
  }
  int
  mulib_marionette_start (mulib_marionette_t *r, uint64_t start)
  {
    const mulib_input_event_t *last;
    if (!r)
      return 0;
    last = (const mulib_input_event_t *)mulib_vec_at (&r->events,
                                                      r->events.length - 1);
    if (last && last->time_us > UINT64_MAX - start)
      return 0;
    r->cursor = 0;
    r->start = start;
    r->playing = r->events.length != 0;
    return 1;
  }
  void
  mulib_marionette_stop (mulib_marionette_t *r)
  {
    if (r)
      r->playing = 0;
  }
  int
  mulib_marionette_tick (mulib_marionette_t *r, uint64_t now, size_t budget,
                         mulib_marionette_sink_fn sink, void *user,
                         size_t *sent)
  {
    size_t count = 0;
    if (sent)
      *sent = 0;
    if (!r || !sink)
      return -1;
    while (r->playing && now >= r->start && (!budget || count < budget))
      {
        const mulib_input_event_t *e
            = (const mulib_input_event_t *)mulib_vec_at (&r->events,
                                                         r->cursor);
        if (!e || e->time_us > now - r->start)
          break;
        if (sink (e, user))
          {
            if (sent)
              *sent = count;
            return -2;
          }
        ++count;
        ++r->cursor;
        if (r->cursor == r->events.length)
          r->playing = 0;
      }
    if (sent)
      *sent = count;
    return 0;
  }
#ifdef __cplusplus
}
#endif
#endif
