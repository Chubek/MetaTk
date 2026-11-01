/**
 * @file mulib-deque.h
 * @brief Double-ended FIFO container with a circular array.
 * @defgroup mulib_deque MuLIB deque
 * @{
 * @details O(1) amortized insertion and removal at either end. The deque owns
 * storage but does not own pointers stored in elements.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_DEQUE before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_DEQUE_H
#define MULIB_DEQUE_H
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

  /** @brief Double-ended FIFO container with a circular array. */
  typedef struct mulib_deque
  {
    unsigned char *data;
    size_t head, length, capacity, stride;
  } mulib_deque_t;
  /** @brief mulib deque init; see manual/mulib-deque.md for contracts and
   * examples. */
  void mulib_deque_init (mulib_deque_t *d, size_t stride);
  /** @brief Copy to the front; sources may be existing aligned live slots,
   * even across growth. */
  int mulib_deque_push_front (mulib_deque_t *d, const void *x);
  /** @brief Copy to the back; sources may be existing aligned live slots, even
   * across growth. */
  int mulib_deque_push_back (mulib_deque_t *d, const void *x);
  /** @brief mulib deque pop front; see manual/mulib-deque.md for contracts and
   * examples. */
  int mulib_deque_pop_front (mulib_deque_t *d, void *out);
  /** @brief mulib deque pop back; see manual/mulib-deque.md for contracts and
   * examples. */
  int mulib_deque_pop_back (mulib_deque_t *d, void *out);
  /** @brief mulib deque free; see manual/mulib-deque.md for contracts and
   * examples. */
  void mulib_deque_free (mulib_deque_t *d);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_DEQUE_H */

#if defined(MULIB_IMPL_DEQUE) && !defined(MULIB_DEQUE_IMPLEMENTED)
#define MULIB_DEQUE_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_deque_init (mulib_deque_t *d, size_t s)
  {
    if (d)
      {
        memset (d, 0, sizeof (*d));
        d->stride = s;
      }
  }
  static size_t
  mulib_deque_index_ (const mulib_deque_t *d, size_t i)
  {
    return i >= d->capacity - d->head ? i - (d->capacity - d->head)
                                      : d->head + i;
  }
  static int
  mulib_deque_grow_ (mulib_deque_t *d, const void **source)
  {
    size_t logical = 0, n;
    int internal = 0;
    unsigned char *p;
    if (!d->stride || d->length == SIZE_MAX || d->length > d->capacity
        || d->capacity > SIZE_MAX / d->stride
        || (d->capacity && (!d->data || d->head >= d->capacity)))
      return 0;
    if (d->data && (uintptr_t)*source >= (uintptr_t)d->data
        && (uintptr_t)*source - (uintptr_t)d->data <= d->capacity * d->stride)
      {
        size_t offset = (size_t)((uintptr_t)*source - (uintptr_t)d->data),
               physical;
        if (offset % d->stride || offset / d->stride >= d->capacity)
          return 0;
        physical = offset / d->stride;
        logical = physical >= d->head ? physical - d->head
                                      : d->capacity - d->head + physical;
        if (logical >= d->length)
          return 0;
        internal = 1;
      }
    if (d->length < d->capacity)
      return 1;
    n = d->capacity && d->capacity <= SIZE_MAX / 2 ? d->capacity * 2
        : d->capacity                              ? d->length + 1
                                                   : 8;
    if (n > SIZE_MAX / d->stride)
      n = d->length + 1;
    if (n > SIZE_MAX / d->stride)
      return 0;
    p = (unsigned char *)malloc (n * d->stride);
    if (!p)
      return 0;
    for (size_t i = 0; i < d->length; ++i)
      memcpy (p + i * d->stride,
              d->data + mulib_deque_index_ (d, i) * d->stride, d->stride);
    free (d->data);
    d->data = p;
    d->head = 0;
    d->capacity = n;
    if (internal)
      *source = p + logical * d->stride;
    return 1;
  }
  int
  mulib_deque_push_back (mulib_deque_t *d, const void *x)
  {
    if (!d || !x || !mulib_deque_grow_ (d, &x))
      return 0;
    memmove (d->data + mulib_deque_index_ (d, d->length) * d->stride, x,
             d->stride);
    d->length++;
    return 1;
  }
  int
  mulib_deque_push_front (mulib_deque_t *d, const void *x)
  {
    if (!d || !x || !mulib_deque_grow_ (d, &x))
      return 0;
    d->head = d->head ? d->head - 1 : d->capacity - 1;
    memmove (d->data + d->head * d->stride, x, d->stride);
    d->length++;
    return 1;
  }
  int
  mulib_deque_pop_front (mulib_deque_t *d, void *out)
  {
    if (!d || !d->length)
      return 0;
    if (out)
      memmove (out, d->data + d->head * d->stride, d->stride);
    d->head = d->head == d->capacity - 1 ? 0 : d->head + 1;
    d->length--;
    return 1;
  }
  int
  mulib_deque_pop_back (mulib_deque_t *d, void *out)
  {
    if (!d || !d->length)
      return 0;
    d->length--;
    if (out)
      memmove (out, d->data + mulib_deque_index_ (d, d->length) * d->stride,
               d->stride);
    return 1;
  }
  void
  mulib_deque_free (mulib_deque_t *d)
  {
    if (d)
      {
        free (d->data);
        memset (d, 0, sizeof (*d));
      }
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
