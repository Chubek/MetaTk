/**
 * @file mulib-emitter.h
 * @brief Bounded, allocation-free endian byte emission and patching.
 * Define MULIB_IMPL_EMITTER in exactly one translation unit.
 */
#ifndef MULIB_EMITTER_H
#define MULIB_EMITTER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_emitter_order
  {
    MULIB_EMITTER_LITTLE,
    MULIB_EMITTER_BIG
  } mulib_emitter_order;
  typedef enum mulib_emitter_result
  {
    MULIB_EMITTER_OK = 0,
    MULIB_EMITTER_EINVAL = -1,
    MULIB_EMITTER_ERANGE = -2,
    MULIB_EMITTER_ENOSPACE = -3
  } mulib_emitter_result;
  /** @brief Borrowed storage; do not modify fields except through this API. */
  typedef struct mulib_emitter
  {
    unsigned char *data;
    size_t size, capacity;
    mulib_emitter_order order;
  } mulib_emitter_t;
#define MULIB_EMITTER_INIT { NULL, 0, 0, MULIB_EMITTER_LITTLE }
  /** @brief Initialize borrowed storage. NULL storage requires zero capacity.
   */
  int mulib_emitter_init (mulib_emitter_t *emitter, void *storage,
                          size_t capacity, mulib_emitter_order order);
  /** @brief Append bytes (overlap allowed). Failure never changes bytes or
   * size. */
  int mulib_emitter_bytes (mulib_emitter_t *emitter, const void *bytes,
                           size_t length);
  /** @brief Append zero-filled bytes; return their starting offset if
   * non-NULL. */
  int mulib_emitter_reserve (mulib_emitter_t *emitter, size_t length,
                             size_t *offset);
  /** @brief Append an unsigned value in 1..8 bytes; reject truncation. */
  int mulib_emitter_uint (mulib_emitter_t *emitter, uint64_t value,
                          unsigned width);
  /** @brief Replace already-emitted bytes (overlap allowed), never extend
   * size. */
  int mulib_emitter_patch (mulib_emitter_t *emitter, size_t offset,
                           const void *bytes, size_t length);
  /** @brief Endian unsigned patch in 1..8 bytes; reject truncation. */
  int mulib_emitter_patch_uint (mulib_emitter_t *emitter, size_t offset,
                                uint64_t value, unsigned width);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_EMITTER) && !defined(MULIB_EMITTER_IMPLEMENTED)
#define MULIB_EMITTER_IMPLEMENTED
#include <limits.h>
#include <string.h>
#if CHAR_BIT != 8
#error "mulib-emitter requires 8-bit bytes"
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_emitter_valid_ (const mulib_emitter_t *e)
  {
    return e && e->size <= e->capacity && (e->data || !e->capacity)
           && (e->order == MULIB_EMITTER_LITTLE
               || e->order == MULIB_EMITTER_BIG);
  }
  int
  mulib_emitter_init (mulib_emitter_t *e, void *storage, size_t cap,
                      mulib_emitter_order order)
  {
    if (!e || (!storage && cap)
        || (order != MULIB_EMITTER_LITTLE && order != MULIB_EMITTER_BIG))
      return MULIB_EMITTER_EINVAL;
    e->data = (unsigned char *)storage;
    e->size = 0;
    e->capacity = cap;
    e->order = order;
    return MULIB_EMITTER_OK;
  }
  int
  mulib_emitter_bytes (mulib_emitter_t *e, const void *bytes, size_t n)
  {
    if (!mulib_emitter_valid_ (e) || (!bytes && n))
      return MULIB_EMITTER_EINVAL;
    if (n > e->capacity - e->size)
      return MULIB_EMITTER_ENOSPACE;
    if (n)
      memmove (e->data + e->size, bytes, n);
    e->size += n;
    return MULIB_EMITTER_OK;
  }
  int
  mulib_emitter_reserve (mulib_emitter_t *e, size_t n, size_t *offset)
  {
    if (!mulib_emitter_valid_ (e))
      return MULIB_EMITTER_EINVAL;
    if (n > e->capacity - e->size)
      return MULIB_EMITTER_ENOSPACE;
    if (offset)
      *offset = e->size;
    if (n)
      memset (e->data + e->size, 0, n);
    e->size += n;
    return MULIB_EMITTER_OK;
  }
  static int
  mulib_emitter_encode_ (const mulib_emitter_t *e, uint64_t v, unsigned w,
                         unsigned char out[8])
  {
    unsigned i;
    if (!mulib_emitter_valid_ (e) || w < 1 || w > 8)
      return MULIB_EMITTER_EINVAL;
    if (w < 8 && v >> (w * 8))
      return MULIB_EMITTER_ERANGE;
    for (i = 0; i < w; ++i)
      {
        out[e->order == MULIB_EMITTER_LITTLE ? i : w - 1 - i]
            = (unsigned char)v;
        v >>= 8;
      }
    return MULIB_EMITTER_OK;
  }
  int
  mulib_emitter_uint (mulib_emitter_t *e, uint64_t v, unsigned w)
  {
    unsigned char bytes[8];
    int result = mulib_emitter_encode_ (e, v, w, bytes);
    return result ? result : mulib_emitter_bytes (e, bytes, w);
  }
  int
  mulib_emitter_patch (mulib_emitter_t *e, size_t at, const void *bytes,
                       size_t n)
  {
    if (!mulib_emitter_valid_ (e) || (!bytes && n))
      return MULIB_EMITTER_EINVAL;
    if (at > e->size || n > e->size - at)
      return MULIB_EMITTER_ERANGE;
    if (n)
      memmove (e->data + at, bytes, n);
    return MULIB_EMITTER_OK;
  }
  int
  mulib_emitter_patch_uint (mulib_emitter_t *e, size_t at, uint64_t v,
                            unsigned w)
  {
    unsigned char bytes[8];
    int result = mulib_emitter_encode_ (e, v, w, bytes);
    return result ? result : mulib_emitter_patch (e, at, bytes, w);
  }
#ifdef __cplusplus
}
#endif
#endif
