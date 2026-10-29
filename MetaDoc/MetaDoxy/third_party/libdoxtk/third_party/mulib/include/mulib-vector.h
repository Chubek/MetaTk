/**
 * @file mulib-vector.h
 * @brief Owned, arbitrary-dimensional double vectors, built on mulib-vec.
 * Define MULIB_IMPL_VECTOR and MULIB_IMPL_VEC once; link the C math library.
 * This is numerical vector algebra, not a second generic container API.
 */
#ifndef MULIB_VECTOR_H
#define MULIB_VECTOR_H
#include "mulib-vec.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_vector { mulib_vec_t elements; } mulib_vector_t;
/** @brief Initialize a fresh object to a zero vector; failure leaves it empty. */
int mulib_vector_init(mulib_vector_t *vector, size_t dimensions);
/** @brief Initialize from copied coordinates; values may be NULL only if n=0. */
int mulib_vector_from(mulib_vector_t *vector, const double *values, size_t dimensions);
/** @brief Release coordinates; the object remains an initialized empty vector. */
void mulib_vector_free(mulib_vector_t *vector);
/** @brief Return the dimension, or zero for NULL. */
size_t mulib_vector_size(const mulib_vector_t *vector);
/** @brief Borrow mutable coordinate storage, or NULL for empty/NULL. */
double *mulib_vector_data(mulib_vector_t *vector);
/** @brief Copy a coordinate; failure leaves *value unchanged. */
int mulib_vector_get(const mulib_vector_t *vector, size_t index, double *value);
/** @brief Replace an indexed coordinate. */
int mulib_vector_set(mulib_vector_t *vector, size_t index, double value);
/** @brief Replace initialized out with a copy; aliasing is supported. */
int mulib_vector_copy(mulib_vector_t *out, const mulib_vector_t *source);
/** @brief Replace initialized out with a+b; all aliases supported. */
int mulib_vector_add(mulib_vector_t *out, const mulib_vector_t *a, const mulib_vector_t *b);
/** @brief Replace initialized out with a-b; all aliases supported. */
int mulib_vector_sub(mulib_vector_t *out, const mulib_vector_t *a, const mulib_vector_t *b);
/** @brief Multiply coordinates in place. IEEE floating-point rules apply. */
int mulib_vector_scale(mulib_vector_t *vector, double scalar);
/** @brief Compute dst += scalar*source; matching dimensions required. */
int mulib_vector_axpy(mulib_vector_t *dst, double scalar, const mulib_vector_t *source);
/** @brief Compute dot product with a long-double accumulator. */
int mulib_vector_dot(const mulib_vector_t *a, const mulib_vector_t *b, double *result);
/** @brief Euclidean norm using hypot to avoid spurious intermediate overflow. */
double mulib_vector_norm(const mulib_vector_t *vector);
/** @brief Normalize in place; reject zero/nonfinite norm without changes. */
int mulib_vector_normalize(mulib_vector_t *vector);
/** @brief Replace initialized out with the 3D cross product; aliases supported. */
int mulib_vector_cross(mulib_vector_t *out, const mulib_vector_t *a, const mulib_vector_t *b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_VECTOR_H */

#if defined(MULIB_IMPL_VECTOR) && !defined(MULIB_VECTOR_IMPLEMENTED)
#define MULIB_VECTOR_IMPLEMENTED
#include <math.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static const double *mulib_vector_values_(const mulib_vector_t *v) {
    return (const double *)(const void *)v->elements.data;
}
int mulib_vector_init(mulib_vector_t *v, size_t n) {
    if (!v) return 0;
    mulib_vec_init(&v->elements, sizeof(double));
    if (!mulib_vec_reserve(&v->elements, n)) return 0;
    if (n) memset(v->elements.data, 0, n * sizeof(double));
    v->elements.length = n;
    return 1;
}
int mulib_vector_from(mulib_vector_t *v, const double *values, size_t n) {
    if (!v) return 0;
    if (!values && n) { mulib_vec_init(&v->elements, sizeof(double)); return 0; }
    if (!mulib_vector_init(v, n)) return 0;
    if (n) memcpy(v->elements.data, values, n * sizeof(double));
    return 1;
}
void mulib_vector_free(mulib_vector_t *v) { if (v) mulib_vec_free(&v->elements); }
size_t mulib_vector_size(const mulib_vector_t *v) { return v ? v->elements.length : 0; }
double *mulib_vector_data(mulib_vector_t *v) { return v ? (double *)(void *)v->elements.data : NULL; }
int mulib_vector_get(const mulib_vector_t *v, size_t i, double *out) {
    if (!v || !out || i >= v->elements.length) return 0;
    *out = mulib_vector_values_(v)[i];
    return 1;
}
int mulib_vector_set(mulib_vector_t *v, size_t i, double value) {
    if (!v || i >= v->elements.length) return 0;
    mulib_vector_data(v)[i] = value;
    return 1;
}
int mulib_vector_copy(mulib_vector_t *out, const mulib_vector_t *v) {
    mulib_vector_t temporary;
    if (!out || !v) return 0;
    if (out == v) return 1;
    if (!mulib_vector_from(&temporary, mulib_vector_values_(v), v->elements.length)) return 0;
    mulib_vector_free(out);
    *out = temporary;
    return 1;
}
static int mulib_vector_combine_(mulib_vector_t *out, const mulib_vector_t *a,
                                 const mulib_vector_t *b, double sign) {
    mulib_vector_t temporary;
    if (!out || !a || !b || a->elements.length != b->elements.length) return 0;
    if (!mulib_vector_init(&temporary, a->elements.length)) return 0;
    for (size_t i = 0; i < a->elements.length; ++i)
        mulib_vector_data(&temporary)[i] = mulib_vector_values_(a)[i] + sign * mulib_vector_values_(b)[i];
    mulib_vector_free(out);
    *out = temporary;
    return 1;
}
int mulib_vector_add(mulib_vector_t *out, const mulib_vector_t *a, const mulib_vector_t *b) {
    return mulib_vector_combine_(out, a, b, 1.0);
}
int mulib_vector_sub(mulib_vector_t *out, const mulib_vector_t *a, const mulib_vector_t *b) {
    return mulib_vector_combine_(out, a, b, -1.0);
}
int mulib_vector_scale(mulib_vector_t *v, double scalar) {
    if (!v) return 0;
    for (size_t i = 0; i < v->elements.length; ++i) mulib_vector_data(v)[i] *= scalar;
    return 1;
}
int mulib_vector_axpy(mulib_vector_t *dst, double scalar, const mulib_vector_t *src) {
    if (!dst || !src || dst->elements.length != src->elements.length) return 0;
    for (size_t i = 0; i < dst->elements.length; ++i)
        mulib_vector_data(dst)[i] += scalar * mulib_vector_values_(src)[i];
    return 1;
}
int mulib_vector_dot(const mulib_vector_t *a, const mulib_vector_t *b, double *out) {
    long double sum = 0;
    if (!a || !b || !out || a->elements.length != b->elements.length) return 0;
    for (size_t i = 0; i < a->elements.length; ++i)
        sum += (long double)mulib_vector_values_(a)[i] * (long double)mulib_vector_values_(b)[i];
    *out = (double)sum;
    return 1;
}
double mulib_vector_norm(const mulib_vector_t *v) {
    double result = 0;
    if (!v) return 0;
    for (size_t i = 0; i < v->elements.length; ++i) result = hypot(result, mulib_vector_values_(v)[i]);
    return result;
}
int mulib_vector_normalize(mulib_vector_t *v) {
    double norm;
    if (!v) return 0;
    norm = mulib_vector_norm(v);
    if (norm == 0 || !isfinite(norm)) return 0;
    for (size_t i = 0; i < v->elements.length; ++i) mulib_vector_data(v)[i] /= norm;
    return 1;
}
int mulib_vector_cross(mulib_vector_t *out, const mulib_vector_t *a, const mulib_vector_t *b) {
    mulib_vector_t temporary;
    const double *x, *y;
    double result[3];
    if (!out || !a || !b || a->elements.length != 3 || b->elements.length != 3) return 0;
    x = mulib_vector_values_(a);
    y = mulib_vector_values_(b);
    result[0] = x[1] * y[2] - x[2] * y[1];
    result[1] = x[2] * y[0] - x[0] * y[2];
    result[2] = x[0] * y[1] - x[1] * y[0];
    if (!mulib_vector_from(&temporary, result, 3)) return 0;
    mulib_vector_free(out);
    *out = temporary;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_VECTOR_IMPLEMENTED */
