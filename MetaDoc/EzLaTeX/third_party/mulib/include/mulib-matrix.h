/**
 * @file mulib-matrix.h
 * @brief Heap-allocated row-major matrix of double precision numbers.
 * @defgroup mulib_matrix MuLIB matrix
 * @{
 * @details Matrix multiplication uses straightforward O(rows × shared
 * dimension × columns) arithmetic with no BLAS dependency. Initialize
 * destination as an unowned empty object; free it after use.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_MATRIX before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_MATRIX_H
#define MULIB_MATRIX_H
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

  /** @brief Heap-allocated row-major matrix of double precision numbers. */
  typedef struct mulib_matrix
  {
    double *data;
    size_t rows, cols;
  } mulib_matrix_t;
  /** @brief mulib matrix init; see manual/mulib-matrix.md for contracts and
   * examples. */
  int mulib_matrix_init (mulib_matrix_t *m, size_t rows, size_t cols);
  /** @brief mulib matrix multiply; see manual/mulib-matrix.md for contracts
   * and examples. */
  int mulib_matrix_multiply (const mulib_matrix_t *a, const mulib_matrix_t *b,
                             mulib_matrix_t *out);
  /** @brief mulib matrix transpose; see manual/mulib-matrix.md for contracts
   * and examples. */
  int mulib_matrix_transpose (const mulib_matrix_t *m, mulib_matrix_t *out);
  /** @brief mulib matrix free; see manual/mulib-matrix.md for contracts and
   * examples. */
  void mulib_matrix_free (mulib_matrix_t *m);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_MATRIX_H */

#if defined(MULIB_IMPL_MATRIX) && !defined(MULIB_MATRIX_IMPLEMENTED)
#define MULIB_MATRIX_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_matrix_init (mulib_matrix_t *m, size_t r, size_t c)
  {
    if (!m || (r && c > SIZE_MAX / r) || r * c > SIZE_MAX / sizeof (double))
      return 0;
    double *p = calloc (r && c ? r * c : 1, sizeof (double));
    if (!p)
      return 0;
    m->data = p;
    m->rows = r;
    m->cols = c;
    return 1;
  }
  void
  mulib_matrix_free (mulib_matrix_t *m)
  {
    if (m)
      {
        free (m->data);
        memset (m, 0, sizeof (*m));
      }
  }
  int
  mulib_matrix_multiply (const mulib_matrix_t *a, const mulib_matrix_t *b,
                         mulib_matrix_t *out)
  {
    if (!a || !b || !out || a->cols != b->rows || out == a || out == b)
      return 0;
    mulib_matrix_t tmp;
    if (!mulib_matrix_init (&tmp, a->rows, b->cols))
      return 0;
    for (size_t i = 0; i < a->rows; i++)
      for (size_t k = 0; k < a->cols; k++)
        for (size_t j = 0; j < b->cols; j++)
          tmp.data[i * tmp.cols + j]
              += a->data[i * a->cols + k] * b->data[k * b->cols + j];
    *out = tmp;
    return 1;
  }
  int
  mulib_matrix_transpose (const mulib_matrix_t *a, mulib_matrix_t *out)
  {
    if (!a || !out || a == out)
      return 0;
    mulib_matrix_t tmp;
    if (!mulib_matrix_init (&tmp, a->cols, a->rows))
      return 0;
    for (size_t i = 0; i < a->rows; i++)
      for (size_t j = 0; j < a->cols; j++)
        tmp.data[j * tmp.cols + i] = a->data[i * a->cols + j];
    *out = tmp;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
