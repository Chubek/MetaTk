/**
 * @file mulib-arena.h
 * @brief Bump allocator with expandable allocation blocks.
 * @defgroup mulib_arena MuLIB arena
 * @{
 * @details Pointer stability is guaranteed until reset. Individual allocations
 * cannot be freed. Allocation aligns to max_align_t and cannot hold
 * over-aligned types.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ARENA before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_ARENA_H
#define MULIB_ARENA_H
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

  /** @brief Bump allocator with expandable allocation blocks. */
  typedef struct mulib_arena_block
  {
    struct mulib_arena_block *next;
    size_t used, capacity;
    max_align_t align;
    unsigned char data[1];
  } mulib_arena_block_t;
  /** @brief Bump allocator with expandable allocation blocks. */
  typedef struct mulib_arena
  {
    mulib_arena_block_t *head;
    size_t block_size;
  } mulib_arena_t;
  /** @brief mulib arena init; see manual/mulib-arena.md for contracts and
   * examples. */
  void mulib_arena_init (mulib_arena_t *a, size_t block_size);
  /** @brief mulib arena alloc; see manual/mulib-arena.md for contracts and
   * examples. */
  void *mulib_arena_alloc (mulib_arena_t *a, size_t size);
  /** @brief mulib arena reset; see manual/mulib-arena.md for contracts and
   * examples. */
  void mulib_arena_reset (mulib_arena_t *a);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ARENA_H */

#if defined(MULIB_IMPL_ARENA) && !defined(MULIB_ARENA_IMPLEMENTED)
#define MULIB_ARENA_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_arena_init (mulib_arena_t *a, size_t n)
  {
    if (a)
      {
        a->head = NULL;
        a->block_size = n ? n : 4096;
      }
  }
  void *
  mulib_arena_alloc (mulib_arena_t *a, size_t n)
  {
    size_t alignment, capacity;
    mulib_arena_block_t *b;
    void *p;
    if (!a)
      return NULL;
#ifdef __cplusplus
    alignment = alignof (max_align_t);
#else
  alignment = _Alignof (max_align_t);
#endif
    if (n > SIZE_MAX - (alignment - 1))
      return NULL;
    n = (n + alignment - 1) / alignment * alignment;
    if (!n)
      n = alignment;
    b = a->head;
    if (b && b->used > b->capacity)
      return NULL;
    if (!b || n > b->capacity - b->used)
      {
        capacity = a->block_size;
        if (capacity < n)
          capacity = n;
        if (capacity > SIZE_MAX - sizeof (*b))
          return NULL;
        b = (mulib_arena_block_t *)malloc (sizeof (*b) + capacity);
        if (!b)
          return NULL;
        b->next = a->head;
        b->used = 0;
        b->capacity = capacity;
        a->head = b;
      }
    p = (unsigned char *)b + offsetof (mulib_arena_block_t, data) + b->used;
    b->used += n;
    return p;
  }
  void
  mulib_arena_reset (mulib_arena_t *a)
  {
    if (!a)
      return;
    mulib_arena_block_t *b = a->head;
    while (b)
      {
        mulib_arena_block_t *n = b->next;
        free (b);
        b = n;
      }
    a->head = NULL;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
