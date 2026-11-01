/**
 * @file mulib-stack.h
 * @brief A last-in-first-out stack of fixed-stride elements.
 * @defgroup mulib_stack MuLIB stack
 * @{
 * @details The default element size is sizeof(void *), so callers can
 * immediately store pointers using mulib_stack_push(stack, &ptr). Choose
 * another stride before the first push.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_STACK before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_STACK_H
#define MULIB_STACK_H
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

  /** @brief A last-in-first-out stack of fixed-stride elements. */
  typedef struct mulib_stack
  {
    unsigned char *data;
    size_t length, capacity, stride;
  } mulib_stack_t;
  /** @brief mulib stack create; see manual/mulib-stack.md for contracts and
   * examples. */
  mulib_stack_t *mulib_stack_create (void);
  /** @brief mulib stack set stride; see manual/mulib-stack.md for contracts
   * and examples. */
  int mulib_stack_set_stride (mulib_stack_t *s, size_t stride);
  /** @brief Copy an element; a borrowed element/peek from this stack is safe
   * across growth. */
  int mulib_stack_push (mulib_stack_t *s, const void *value);
  /** @brief mulib stack pop; see manual/mulib-stack.md for contracts and
   * examples. */
  int mulib_stack_pop (mulib_stack_t *s, void *out);
  /** @brief mulib stack peek; see manual/mulib-stack.md for contracts and
   * examples. */
  const void *mulib_stack_peek (const mulib_stack_t *s);
  /** @brief mulib stack destroy; see manual/mulib-stack.md for contracts and
   * examples. */
  void mulib_stack_destroy (mulib_stack_t *s);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_STACK_H */

#if defined(MULIB_IMPL_STACK) && !defined(MULIB_STACK_IMPLEMENTED)
#define MULIB_STACK_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_stack_t *
  mulib_stack_create (void)
  {
    mulib_stack_t *s = calloc (1, sizeof (*s));
    if (s)
      s->stride = sizeof (void *);
    return s;
  }
  int
  mulib_stack_set_stride (mulib_stack_t *s, size_t n)
  {
    if (!s || !n || s->length)
      return 0;
    free (s->data);
    s->data = NULL;
    s->capacity = 0;
    s->stride = n;
    return 1;
  }
  int
  mulib_stack_push (mulib_stack_t *s, const void *v)
  {
    size_t offset = 0;
    int internal = 0;
    if (!s || !v || !s->stride || s->length == SIZE_MAX
        || s->length > s->capacity || s->capacity > SIZE_MAX / s->stride
        || (!s->data && s->capacity) || s->length + 1 > SIZE_MAX / s->stride)
      return 0;
    if (s->data && (uintptr_t)v >= (uintptr_t)s->data
        && (uintptr_t)v - (uintptr_t)s->data <= s->capacity * s->stride)
      {
        offset = (size_t)((uintptr_t)v - (uintptr_t)s->data);
        if (offset > s->length * s->stride
            || s->stride > s->length * s->stride - offset)
          return 0;
        internal = 1;
      }
    if (s->length == s->capacity)
      {
        size_t n = s->capacity && s->capacity <= SIZE_MAX / 2 ? s->capacity * 2
                   : s->capacity                              ? s->length + 1
                                                              : 8;
        void *p;
        if (n > SIZE_MAX / s->stride)
          n = s->length + 1;
        p = realloc (s->data, n * s->stride);
        if (!p)
          return 0;
        s->data = p;
        s->capacity = n;
      }
    if (internal)
      v = s->data + offset;
    memmove (s->data + s->length * s->stride, v, s->stride);
    ++s->length;
    return 1;
  }
  int
  mulib_stack_pop (mulib_stack_t *s, void *out)
  {
    if (!s || !s->length)
      return 0;
    --s->length;
    if (out)
      memmove (out, s->data + s->length * s->stride, s->stride);
    return 1;
  }
  const void *
  mulib_stack_peek (const mulib_stack_t *s)
  {
    return s && s->length ? s->data + (s->length - 1) * s->stride : NULL;
  }
  void
  mulib_stack_destroy (mulib_stack_t *s)
  {
    if (s)
      {
        free (s->data);
        free (s);
      }
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
