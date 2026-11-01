/**
 * @file mulib-diag.h
 * @brief Small status-code-to-text mapping for library diagnostics.
 * @defgroup mulib_diag MuLIB diag
 * @{
 * @details Defines general error categories without imposing them on other
 * modules or promising a global errno convention.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_DIAG before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_DIAG_H
#define MULIB_DIAG_H
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

  /** @brief Small status-code-to-text mapping for library diagnostics. */
  typedef enum mulib_diag_code
  {
    MULIB_DIAG_OK = 0,
    MULIB_DIAG_ARGUMENT = 1,
    MULIB_DIAG_MEMORY = 2,
    MULIB_DIAG_PARSE = 3,
    MULIB_DIAG_IO = 4,
    MULIB_DIAG_OVERFLOW = 5
  } mulib_diag_code_t;
  /** @brief mulib diag message; see manual/mulib-diag.md for contracts and
   * examples. */
  const char *mulib_diag_message (mulib_diag_code_t code);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_DIAG_H */

#if defined(MULIB_IMPL_DIAG) && !defined(MULIB_DIAG_IMPLEMENTED)
#define MULIB_DIAG_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  const char *
  mulib_diag_message (mulib_diag_code_t c)
  {
    switch (c)
      {
      case MULIB_DIAG_OK:
        return "success";
      case MULIB_DIAG_ARGUMENT:
        return "invalid argument";
      case MULIB_DIAG_MEMORY:
        return "out of memory";
      case MULIB_DIAG_PARSE:
        return "parse failure";
      case MULIB_DIAG_IO:
        return "I/O failure";
      case MULIB_DIAG_OVERFLOW:
        return "arithmetic overflow";
      default:
        return "unknown error";
      }
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
