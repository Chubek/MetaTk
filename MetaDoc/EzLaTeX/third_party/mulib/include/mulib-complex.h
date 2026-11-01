/**
 * @file mulib-complex.h
 * @brief Complex arithmetic for double precision real and imaginary
 * components.
 * @defgroup mulib_complex MuLIB complex
 * @{
 * @details This type is a plain struct intended for interoperability with C
 * programs that do not rely on C99 _Complex. Use the platform libm for hypot.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_COMPLEX before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_COMPLEX_H
#define MULIB_COMPLEX_H
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

  /** @brief Complex arithmetic for double precision real and imaginary
   * components. */
  typedef struct mulib_complex
  {
    double real, imag;
  } mulib_complex_t;
  /** @brief mulib complex add; see manual/mulib-complex.md for contracts and
   * examples. */
  mulib_complex_t mulib_complex_add (mulib_complex_t a, mulib_complex_t b);
  /** @brief mulib complex mul; see manual/mulib-complex.md for contracts and
   * examples. */
  mulib_complex_t mulib_complex_mul (mulib_complex_t a, mulib_complex_t b);
  /** @brief mulib complex abs; see manual/mulib-complex.md for contracts and
   * examples. */
  double mulib_complex_abs (mulib_complex_t z);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_COMPLEX_H */

#if defined(MULIB_IMPL_COMPLEX) && !defined(MULIB_COMPLEX_IMPLEMENTED)
#define MULIB_COMPLEX_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_complex_t
  mulib_complex_add (mulib_complex_t a, mulib_complex_t b)
  {
    return (mulib_complex_t){ a.real + b.real, a.imag + b.imag };
  }
  mulib_complex_t
  mulib_complex_mul (mulib_complex_t a, mulib_complex_t b)
  {
    return (mulib_complex_t){ a.real * b.real - a.imag * b.imag,
                              a.real * b.imag + a.imag * b.real };
  }
  double
  mulib_complex_abs (mulib_complex_t a)
  {
    return hypot (a.real, a.imag);
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
