/**
 * @file mulib-logger.h
 * @brief Simple callback-driven logging with message severity filtering.
 * @defgroup mulib_logger MuLIB logger
 * @{
 * @details Logging does not allocate, format printf arguments, or access
 * global state. Caller owns the sink function and user pointer.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_LOGGER before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_LOGGER_H
#define MULIB_LOGGER_H
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

  /** @brief Simple callback-driven logging with message severity filtering. */
  typedef enum mulib_logger_level
  {
    MULIB_LOG_DEBUG = 0,
    MULIB_LOG_INFO = 1,
    MULIB_LOG_WARN = 2,
    MULIB_LOG_ERROR = 3
  } mulib_logger_level_t;
  /** @brief Simple callback-driven logging with message severity filtering. */
  typedef void (*mulib_logger_sink) (void *user, mulib_logger_level_t level,
                                     const char *message);
  /** @brief Simple callback-driven logging with message severity filtering. */
  typedef struct mulib_logger
  {
    mulib_logger_sink sink;
    void *user;
    mulib_logger_level_t minimum;
  } mulib_logger_t;
  /** @brief mulib logger init; see manual/mulib-logger.md for contracts and
   * examples. */
  void mulib_logger_init (mulib_logger_t *l, mulib_logger_sink sink,
                          void *user, mulib_logger_level_t minimum);
  /** @brief mulib logger write; see manual/mulib-logger.md for contracts and
   * examples. */
  int mulib_logger_write (mulib_logger_t *l, mulib_logger_level_t level,
                          const char *message);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_LOGGER_H */

#if defined(MULIB_IMPL_LOGGER) && !defined(MULIB_LOGGER_IMPLEMENTED)
#define MULIB_LOGGER_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_logger_init (mulib_logger_t *l, mulib_logger_sink cb, void *user,
                     mulib_logger_level_t lvl)
  {
    if (l)
      {
        l->sink = cb;
        l->user = user;
        l->minimum = lvl;
      }
  }
  int
  mulib_logger_write (mulib_logger_t *l, mulib_logger_level_t lvl,
                      const char *s)
  {
    if (!l || !s || !l->sink)
      return 0;
    if (lvl >= l->minimum)
      l->sink (l->user, lvl, s);
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
