/**
 * @file mulib-sort.h
 * @brief Standard-library-backed generic sorting and binary lookup.
 * @defgroup mulib_sort MuLIB sort
 * @{
 * @details Sorting uses the platform C runtime qsort algorithm; the order of
 * equal elements is not stable. Binary lookup requires a sorted array with the
 * same comparison relation.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_SORT before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_SORT_H
#define MULIB_SORT_H
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

  /** @brief mulib sort array; see manual/mulib-sort.md for contracts and
   * examples. */
  void mulib_sort_array (void *base, size_t count, size_t stride,
                         int (*compare) (const void *, const void *));
  /** @brief mulib sort binary; see manual/mulib-sort.md for contracts and
   * examples. */
  void *mulib_sort_binary (const void *key, const void *base, size_t count,
                           size_t stride,
                           int (*compare) (const void *, const void *));

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SORT_H */

#if defined(MULIB_IMPL_SORT) && !defined(MULIB_SORT_IMPLEMENTED)
#define MULIB_SORT_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_sort_array (void *p, size_t n, size_t z,
                    int (*cmp) (const void *, const void *))
  {
    if (p && z && cmp && n <= SIZE_MAX / z)
      qsort (p, n, z, cmp);
  }
  void *
  mulib_sort_binary (const void *k, const void *p, size_t n, size_t z,
                     int (*cmp) (const void *, const void *))
  {
    return k && p && z && cmp && n <= SIZE_MAX / z ? bsearch (k, p, n, z, cmp)
                                                   : NULL;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
