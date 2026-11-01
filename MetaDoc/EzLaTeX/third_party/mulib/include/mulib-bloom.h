/**
 * @file mulib-bloom.h
 * @brief Bit-packed Bloom filter with configurable hash rounds.
 * @defgroup mulib_bloom MuLIB bloom
 * @{
 * @details Bloom filters never return false negatives for inserted items,
 * assuming the storage remains unchanged, but can return false positives. Hash
 * function is non-cryptographic.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_BLOOM before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_BLOOM_H
#define MULIB_BLOOM_H
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

  /** @brief Bit-packed Bloom filter with configurable hash rounds. */
  typedef struct mulib_bloom
  {
    unsigned char *bits;
    size_t bit_count;
    unsigned rounds;
  } mulib_bloom_t;
  /** @brief mulib bloom init; see manual/mulib-bloom.md for contracts and
   * examples. */
  int mulib_bloom_init (mulib_bloom_t *b, size_t bits, unsigned rounds);
  /** @brief mulib bloom add; see manual/mulib-bloom.md for contracts and
   * examples. */
  void mulib_bloom_add (mulib_bloom_t *b, const void *data, size_t n);
  /** @brief mulib bloom test; see manual/mulib-bloom.md for contracts and
   * examples. */
  int mulib_bloom_test (const mulib_bloom_t *b, const void *data, size_t n);
  /** @brief mulib bloom free; see manual/mulib-bloom.md for contracts and
   * examples. */
  void mulib_bloom_free (mulib_bloom_t *b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_BLOOM_H */

#if defined(MULIB_IMPL_BLOOM) && !defined(MULIB_BLOOM_IMPLEMENTED)
#define MULIB_BLOOM_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  static uint64_t
  mulib_bloom_hash_ (const void *p, size_t n, uint64_t seed)
  {
    const unsigned char *s = p;
    uint64_t h = seed;
    for (size_t i = 0; i < n; i++)
      {
        h ^= s[i];
        h *= UINT64_C (1099511628211);
      }
    return h;
  }
  int
  mulib_bloom_init (mulib_bloom_t *b, size_t bits, unsigned k)
  {
    if (!b || !bits || !k || bits > SIZE_MAX - 7)
      return 0;
    unsigned char *p = calloc ((bits + 7) / 8, 1);
    if (!p)
      return 0;
    b->bits = p;
    b->bit_count = bits;
    b->rounds = k;
    return 1;
  }
  void
  mulib_bloom_add (mulib_bloom_t *b, const void *p, size_t n)
  {
    if (!b || !b->bits || (!p && n))
      return;
    uint64_t a = mulib_bloom_hash_ (p, n, UINT64_C (14695981039346656037)),
             h = mulib_bloom_hash_ (p, n, UINT64_C (7809847782465536322)) | 1;
    for (unsigned i = 0; i < b->rounds; i++)
      {
        size_t bit = (size_t)((a + i * h) % b->bit_count);
        b->bits[bit / 8] |= (unsigned char)(1u << (bit % 8));
      }
  }
  int
  mulib_bloom_test (const mulib_bloom_t *b, const void *p, size_t n)
  {
    if (!b || !b->bits || (!p && n))
      return 0;
    uint64_t a = mulib_bloom_hash_ (p, n, UINT64_C (14695981039346656037)),
             h = mulib_bloom_hash_ (p, n, UINT64_C (7809847782465536322)) | 1;
    for (unsigned i = 0; i < b->rounds; i++)
      {
        size_t bit = (size_t)((a + i * h) % b->bit_count);
        if (!(b->bits[bit / 8] & (1u << (bit % 8))))
          return 0;
      }
    return 1;
  }
  void
  mulib_bloom_free (mulib_bloom_t *b)
  {
    if (b)
      {
        free (b->bits);
        memset (b, 0, sizeof (*b));
      }
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
