/**
 * @file mulib-dyncall.h
 * @brief Checked invocation of exactly typed native C functions, without
 * casts. Define MULIB_IMPL_DYNCALL once. This is not a general FFI or ABI
 * adapter.
 */
#ifndef MULIB_DYNCALL_H
#define MULIB_DYNCALL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef int64_t (*mulib_dyncall_i64_0_fn) (void);
  typedef int64_t (*mulib_dyncall_i64_1_fn) (int64_t);
  typedef int64_t (*mulib_dyncall_i64_2_fn) (int64_t, int64_t);
  typedef double (*mulib_dyncall_f64_0_fn) (void);
  typedef double (*mulib_dyncall_f64_1_fn) (double);
  typedef double (*mulib_dyncall_f64_2_fn) (double, double);
  /** @brief Call an exact int64_t(void) function; NULL function/output fails.
   */
  int mulib_dyncall_i64_0 (mulib_dyncall_i64_0_fn function, int64_t *result);
  /** @brief Call an exact int64_t(int64_t) function; result unchanged on
   * failure. */
  int mulib_dyncall_i64_1 (mulib_dyncall_i64_1_fn function, int64_t a,
                           int64_t *result);
  /** @brief Call an exact int64_t(int64_t,int64_t) native C function. */
  int mulib_dyncall_i64_2 (mulib_dyncall_i64_2_fn function, int64_t a,
                           int64_t b, int64_t *result);
  /** @brief Call an exact double(void) native C function. */
  int mulib_dyncall_f64_0 (mulib_dyncall_f64_0_fn function, double *result);
  /** @brief Call an exact double(double) native C function. */
  int mulib_dyncall_f64_1 (mulib_dyncall_f64_1_fn function, double a,
                           double *result);
  /** @brief Call an exact double(double,double) native C function. */
  int mulib_dyncall_f64_2 (mulib_dyncall_f64_2_fn function, double a, double b,
                           double *result);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_DYNCALL) && !defined(MULIB_DYNCALL_IMPLEMENTED)
#define MULIB_DYNCALL_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_dyncall_i64_0 (mulib_dyncall_i64_0_fn f, int64_t *out)
  {
    if (!f || !out)
      return -1;
    *out = f ();
    return 0;
  }
  int
  mulib_dyncall_i64_1 (mulib_dyncall_i64_1_fn f, int64_t a, int64_t *out)
  {
    if (!f || !out)
      return -1;
    *out = f (a);
    return 0;
  }
  int
  mulib_dyncall_i64_2 (mulib_dyncall_i64_2_fn f, int64_t a, int64_t b,
                       int64_t *out)
  {
    if (!f || !out)
      return -1;
    *out = f (a, b);
    return 0;
  }
  int
  mulib_dyncall_f64_0 (mulib_dyncall_f64_0_fn f, double *out)
  {
    if (!f || !out)
      return -1;
    *out = f ();
    return 0;
  }
  int
  mulib_dyncall_f64_1 (mulib_dyncall_f64_1_fn f, double a, double *out)
  {
    if (!f || !out)
      return -1;
    *out = f (a);
    return 0;
  }
  int
  mulib_dyncall_f64_2 (mulib_dyncall_f64_2_fn f, double a, double b,
                       double *out)
  {
    if (!f || !out)
      return -1;
    *out = f (a, b);
    return 0;
  }
#ifdef __cplusplus
}
#endif
#endif
