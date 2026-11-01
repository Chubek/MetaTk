/**
 * @file mulib-queue.h
 * @brief A dynamically growing FIFO queue implemented with a circular buffer.
 * @defgroup mulib_queue MuLIB queue
 * @{
 * @details The queue preserves insertion order even after wraparound and
 * relocation. Push may invalidate pointers into its storage.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_QUEUE before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_QUEUE_H
#define MULIB_QUEUE_H
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

  /** @brief A dynamically growing FIFO queue implemented with a circular
   * buffer. */
  typedef struct mulib_queue
  {
    unsigned char *data;
    size_t head, length, capacity, stride;
  } mulib_queue_t;
  /** @brief mulib queue init; see manual/mulib-queue.md for contracts and
   * examples. */
  void mulib_queue_init (mulib_queue_t *q, size_t stride);
  /** @brief Copy an element; existing aligned live slots may be sources across
   * growth. */
  int mulib_queue_push (mulib_queue_t *q, const void *item);
  /** @brief mulib queue pop; see manual/mulib-queue.md for contracts and
   * examples. */
  int mulib_queue_pop (mulib_queue_t *q, void *out);
  /** @brief mulib queue free; see manual/mulib-queue.md for contracts and
   * examples. */
  void mulib_queue_free (mulib_queue_t *q);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_QUEUE_H */

#if defined(MULIB_IMPL_QUEUE) && !defined(MULIB_QUEUE_IMPLEMENTED)
#define MULIB_QUEUE_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_queue_init (mulib_queue_t *q, size_t s)
  {
    if (q)
      {
        memset (q, 0, sizeof (*q));
        q->stride = s;
      }
  }
  int
  mulib_queue_push (mulib_queue_t *q, const void *x)
  {
    size_t logical = 0;
    int internal = 0;
    if (!q || !x || !q->stride || q->length == SIZE_MAX
        || q->length > q->capacity || q->capacity > SIZE_MAX / q->stride
        || (q->capacity && (!q->data || q->head >= q->capacity)))
      return 0;
    if (q->data && (uintptr_t)x >= (uintptr_t)q->data
        && (uintptr_t)x - (uintptr_t)q->data <= q->capacity * q->stride)
      {
        size_t offset = (size_t)((uintptr_t)x - (uintptr_t)q->data), physical;
        if (offset % q->stride || offset / q->stride >= q->capacity)
          return 0;
        physical = offset / q->stride;
        logical = physical >= q->head ? physical - q->head
                                      : q->capacity - q->head + physical;
        if (logical >= q->length)
          return 0;
        internal = 1;
      }
    if (q->length == q->capacity)
      {
        size_t n = q->capacity && q->capacity <= SIZE_MAX / 2 ? q->capacity * 2
                   : q->capacity                              ? q->length + 1
                                                              : 8;
        unsigned char *p;
        if (n > SIZE_MAX / q->stride)
          n = q->length + 1;
        if (n > SIZE_MAX / q->stride)
          return 0;
        p = (unsigned char *)malloc (n * q->stride);
        if (!p)
          return 0;
        for (size_t i = 0; i < q->length; ++i)
          {
            size_t at = i >= q->capacity - q->head
                            ? i - (q->capacity - q->head)
                            : q->head + i;
            memcpy (p + i * q->stride, q->data + at * q->stride, q->stride);
          }
        free (q->data);
        q->data = p;
        q->head = 0;
        q->capacity = n;
        if (internal)
          x = p + logical * q->stride;
      }
    {
      size_t at = q->length >= q->capacity - q->head
                      ? q->length - (q->capacity - q->head)
                      : q->head + q->length;
      memmove (q->data + at * q->stride, x, q->stride);
    }
    ++q->length;
    return 1;
  }
  int
  mulib_queue_pop (mulib_queue_t *q, void *out)
  {
    if (!q || !q->length)
      return 0;
    if (out)
      memmove (out, q->data + q->head * q->stride, q->stride);
    q->head = q->head == q->capacity - 1 ? 0 : q->head + 1;
    q->length--;
    return 1;
  }
  void
  mulib_queue_free (mulib_queue_t *q)
  {
    if (q)
      {
        free (q->data);
        memset (q, 0, sizeof (*q));
      }
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
