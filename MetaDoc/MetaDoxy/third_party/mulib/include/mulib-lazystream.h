/** @file mulib-lazystream.h
 * @brief Single-pass lazy functional streams of borrowed void* values.
 * Define MULIB_IMPL_LAZYSTREAM. No work occurs until next/fold/foreach.
 * Adapters take ownership of upstream ONLY on successful construction. Contexts
 * and yielded values are borrowed; a source may supply a context destructor.
 * NULL is a valid element. Streams are not thread safe, cloneable or memoized.
 */
#ifndef MULIB_LAZYSTREAM_H
#define MULIB_LAZYSTREAM_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_lazystream mulib_lazystream_t;
/** 1 element, 0 end, -1 error; terminal statuses are sticky. */
typedef int (*mulib_lazy_next_fn)(void *context, void **value);
typedef void (*mulib_lazy_destroy_fn)(void *context);
typedef int (*mulib_lazy_map_fn)(void *value, void **mapped, void *context);
/** Predicate: 1 keep, 0 reject, -1 error. */
typedef int (*mulib_lazy_predicate_fn)(void *value, void *context);
/** Fold: return nonzero to continue, zero to abort with error. */
typedef int (*mulib_lazy_fold_fn)(void *accumulator, void *value, void *context);
mulib_lazystream_t *mulib_lazystream_source(mulib_lazy_next_fn next, void *context, mulib_lazy_destroy_fn destroy);
mulib_lazystream_t *mulib_lazystream_array(void *const *values, size_t count);
mulib_lazystream_t *mulib_lazystream_map(mulib_lazystream_t *upstream, mulib_lazy_map_fn map, void *context);
mulib_lazystream_t *mulib_lazystream_filter(mulib_lazystream_t *upstream, mulib_lazy_predicate_fn predicate, void *context);
mulib_lazystream_t *mulib_lazystream_take(mulib_lazystream_t *upstream, size_t count);
mulib_lazystream_t *mulib_lazystream_drop(mulib_lazystream_t *upstream, size_t count);
int mulib_lazystream_next(mulib_lazystream_t *stream, void **value);
/** Consume the remaining stream. Returns 1 on exhaustion, -1 on error. */
int mulib_lazystream_fold(mulib_lazystream_t *stream, void *accumulator, mulib_lazy_fold_fn fold, void *context);
void mulib_lazystream_destroy(mulib_lazystream_t *stream);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LAZYSTREAM) && !defined(MULIB_LAZYSTREAM_IMPLEMENTED)
#define MULIB_LAZYSTREAM_IMPLEMENTED
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_lazystream {
    int kind, terminal, ended;
    struct mulib_lazystream *upstream;
    void *context;
    mulib_lazy_next_fn source;
    mulib_lazy_destroy_fn cleanup;
    mulib_lazy_map_fn map;
    mulib_lazy_predicate_fn predicate;
    void *const *array;
    size_t count, index;
};
static mulib_lazystream_t *mulib_lazy_new_(int kind, mulib_lazystream_t *upstream) {
    mulib_lazystream_t *s = (mulib_lazystream_t *)calloc(1, sizeof *s);
    if (s) { s->kind = kind; s->upstream = upstream; }
    return s;
}
mulib_lazystream_t *mulib_lazystream_source(mulib_lazy_next_fn f, void *u, mulib_lazy_destroy_fn d) {
    mulib_lazystream_t *s;
    if (!f) return NULL;
    s = mulib_lazy_new_(0, NULL);
    if (s) { s->source = f; s->context = u; s->cleanup = d; }
    return s;
}
mulib_lazystream_t *mulib_lazystream_array(void *const *a, size_t n) {
    mulib_lazystream_t *s;
    if (!a && n) return NULL;
    s = mulib_lazy_new_(1, NULL);
    if (s) { s->array = a; s->count = n; }
    return s;
}
mulib_lazystream_t *mulib_lazystream_map(mulib_lazystream_t *up, mulib_lazy_map_fn f, void *u) {
    mulib_lazystream_t *s;
    if (!up || !f) return NULL;
    s = mulib_lazy_new_(2, up);
    if (s) { s->map = f; s->context = u; }
    return s;
}
mulib_lazystream_t *mulib_lazystream_filter(mulib_lazystream_t *up, mulib_lazy_predicate_fn f, void *u) {
    mulib_lazystream_t *s;
    if (!up || !f) return NULL;
    s = mulib_lazy_new_(3, up);
    if (s) { s->predicate = f; s->context = u; }
    return s;
}
static mulib_lazystream_t *mulib_lazy_count_(mulib_lazystream_t *up, size_t n, int kind) {
    mulib_lazystream_t *s;
    if (!up) return NULL;
    s = mulib_lazy_new_(kind, up);
    if (s) s->count = n;
    return s;
}
mulib_lazystream_t *mulib_lazystream_take(mulib_lazystream_t *up, size_t n) { return mulib_lazy_count_(up, n, 4); }
mulib_lazystream_t *mulib_lazystream_drop(mulib_lazystream_t *up, size_t n) { return mulib_lazy_count_(up, n, 5); }
int mulib_lazystream_next(mulib_lazystream_t *s, void **out) {
    int r = 0;
    void *v = NULL;
    if (!s || !out) return -1;
    *out = NULL;
    if (s->ended) return s->terminal;
    switch (s->kind) {
    case 0: r = s->source(s->context, &v); break;
    case 1: if (s->index < s->count) { v = s->array[s->index++]; r = 1; } break;
    case 2:
        r = mulib_lazystream_next(s->upstream, &v);
        if (r == 1 && s->map(v, &v, s->context) != 1) r = -1;
        break;
    case 3:
        while ((r = mulib_lazystream_next(s->upstream, &v)) == 1) {
            int keep = s->predicate(v, s->context);
            if (keep == 1) break;
            if (keep != 0) { r = -1; break; }
        }
        break;
    case 4:
        if (s->count) { --s->count; r = mulib_lazystream_next(s->upstream, &v); }
        break;
    case 5:
        while (s->count) {
            r = mulib_lazystream_next(s->upstream, &v);
            if (r != 1) break;
            --s->count;
        }
        if (!s->count) r = mulib_lazystream_next(s->upstream, &v);
        break;
    default: r = -1; break;
    }
    if (r == 1) *out = v;
    else { s->ended = 1; s->terminal = r == 0 ? 0 : -1; r = s->terminal; }
    return r;
}
int mulib_lazystream_fold(mulib_lazystream_t *s, void *acc, mulib_lazy_fold_fn f, void *u) {
    void *v;
    int r;
    if (!s || !f) return -1;
    while ((r = mulib_lazystream_next(s, &v)) == 1) if (!f(acc, v, u)) return -1;
    return r == 0 ? 1 : -1;
}
void mulib_lazystream_destroy(mulib_lazystream_t *s) {
    while (s) {
        mulib_lazystream_t *up = s->upstream;
        if (s->cleanup) s->cleanup(s->context);
        free(s); s = up;
    }
}
#ifdef __cplusplus
}
#endif
#endif
