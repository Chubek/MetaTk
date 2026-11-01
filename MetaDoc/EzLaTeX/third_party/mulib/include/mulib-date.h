/**
 * @file mulib-date.h
 * @brief Gregorian leap-year and calendar-days arithmetic.
 * @defgroup mulib_date MuLIB date
 * @{
 * @details Uses the proleptic Gregorian calendar for integer years; does not
 * model time zones, leap seconds, or historical calendar transitions.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_DATE before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_DATE_H
#define MULIB_DATE_H
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

  /** @brief mulib date is leap; see manual/mulib-date.md for contracts and
   * examples. */
  int mulib_date_is_leap (int year);
  /** @brief mulib date days in month; see manual/mulib-date.md for contracts
   * and examples. */
  unsigned mulib_date_days_in_month (int year, unsigned month);
  /** @brief mulib date valid; see manual/mulib-date.md for contracts and
   * examples. */
  int mulib_date_valid (int year, unsigned month, unsigned day);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_DATE_H */

#if defined(MULIB_IMPL_DATE) && !defined(MULIB_DATE_IMPLEMENTED)
#define MULIB_DATE_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_date_is_leap (int y)
  {
    return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
  }
  unsigned
  mulib_date_days_in_month (int y, unsigned m)
  {
    static const unsigned days[]
        = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (!m || m > 12)
      return 0;
    return days[m] + (m == 2 && mulib_date_is_leap (y));
  }
  int
  mulib_date_valid (int y, unsigned m, unsigned d)
  {
    unsigned lim = mulib_date_days_in_month (y, m);
    return d > 0 && d <= lim;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
