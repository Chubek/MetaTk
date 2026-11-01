/**
 * @file mulib-histogram.h
 * @brief Histogram accumulation over a fixed numeric range.
 * @defgroup mulib_histogram MuLIB histogram
 * @{
 * @details Values outside [low, high] are rejected, and the high endpoint
 * belongs to the final bin. Widths are uniform in floating-point units.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_HISTOGRAM before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_HISTOGRAM_H
#define MULIB_HISTOGRAM_H
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

  /** @brief Histogram accumulation over a fixed numeric range. */
  typedef struct mulib_histogram
  {
    size_t *bins;
    size_t count;
    double low, high;
  } mulib_histogram_t;
  /** @brief Allocate bins over finite low < high; failure leaves the
   * destination unchanged. */
  int mulib_histogram_init (mulib_histogram_t *h, size_t bins, double low,
                            double high);
  /** @brief mulib histogram add; see manual/mulib-histogram.md for contracts
   * and examples. */
  int mulib_histogram_add (mulib_histogram_t *h, double value);
  /** @brief mulib histogram free; see manual/mulib-histogram.md for contracts
   * and examples. */
  void mulib_histogram_free (mulib_histogram_t *h);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_HISTOGRAM_H */

#if defined(MULIB_IMPL_HISTOGRAM) && !defined(MULIB_HISTOGRAM_IMPLEMENTED)
#define MULIB_HISTOGRAM_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_histogram_init (mulib_histogram_t *h, size_t n, double lo, double hi)
  {
    if (!h || !n || !isfinite (lo) || !isfinite (hi) || !(lo < hi)
        || n > SIZE_MAX / sizeof (size_t))
      return 0;
    size_t *p = calloc (n, sizeof (size_t));
    if (!p)
      return 0;
    h->bins = p;
    h->count = n;
    h->low = lo;
    h->high = hi;
    return 1;
  }
  int
  mulib_histogram_add (mulib_histogram_t *h, double x)
  {
    double span, fraction, position;
    size_t i;
    if (!h || !h->bins || !h->count || !isfinite (x) || x < h->low
        || x > h->high)
      return 0;
    span = h->high - h->low;
    fraction = isfinite (span)
                   ? (x - h->low) / span
                   : (x / 2 - h->low / 2) / (h->high / 2 - h->low / 2);
    position = fraction * (double)h->count;
    i = position >= (double)h->count ? h->count - 1 : (size_t)position;
    if (h->bins[i] == SIZE_MAX)
      return 0;
    ++h->bins[i];
    return 1;
  }
  void
  mulib_histogram_free (mulib_histogram_t *h)
  {
    if (h)
      {
        free (h->bins);
        memset (h, 0, sizeof (*h));
      }
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
