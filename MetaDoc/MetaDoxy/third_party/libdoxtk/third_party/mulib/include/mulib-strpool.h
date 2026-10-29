/**
 * @file mulib-strpool.h
 * @brief Binary-safe string interning with stable addresses and hashed lookup.
 * Define MULIB_IMPL_STRPOOL, MULIB_IMPL_ALLOC, MULIB_IMPL_PORT,
 * MULIB_IMPL_HASH once. Interned pointers survive growth, not reset/destroy.
 */
#ifndef MULIB_STRPOOL_H
#define MULIB_STRPOOL_H
#include "mulib-alloc.h"
#include "mulib-hash.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_strpool mulib_strpool_t;
/** @brief Create an initially empty pool; copy allocator callbacks. */
mulib_strpool_t *mulib_strpool_create(const mulib_allocator_t *allocator);
/** @brief Intern a NUL-terminated string, returning a pool-owned pointer. */
const char *mulib_strpool_intern(mulib_strpool_t *pool, const char *text);
/** @brief Intern length-delimited bytes, appending a convenience NUL. */
const char *mulib_strpool_intern_n(mulib_strpool_t *pool, const char *text, size_t length);
/** @brief Find without inserting; NULL means absent or invalid arguments. */
const char *mulib_strpool_find_n(const mulib_strpool_t *pool, const char *text, size_t length);
/** @brief Reserve space for at least count unique strings without rehashing. */
int mulib_strpool_reserve(mulib_strpool_t *pool, size_t count);
/** @brief Return unique string count. */
size_t mulib_strpool_size(const mulib_strpool_t *pool);
/** @brief Return summed unique byte lengths, excluding convenience NULs. */
size_t mulib_strpool_bytes(const mulib_strpool_t *pool);
/** @brief Release interned strings, retaining bucket capacity. */
void mulib_strpool_reset(mulib_strpool_t *pool);
/** @brief Release all resources. */
void mulib_strpool_destroy(mulib_strpool_t *pool);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_STRPOOL_H */

#if defined(MULIB_IMPL_STRPOOL) && !defined(MULIB_STRPOOL_IMPLEMENTED)
#define MULIB_STRPOOL_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_strpool_entry_ {
    struct mulib_strpool_entry_ *next;
    uint64_t hash;
    size_t length;
    char *text;
} mulib_strpool_entry_;
struct mulib_strpool {
    mulib_allocator_t allocator;
    mulib_strpool_entry_ **buckets;
    size_t capacity, count, bytes;
};
mulib_strpool_t *mulib_strpool_create(const mulib_allocator_t *a) {
    mulib_strpool_t *p;
    if (!mulib_alloc_valid(a)) return NULL;
    p = (mulib_strpool_t *)mulib_alloc_calloc(a, 1, sizeof(*p));
    if (p) p->allocator = a ? *a : mulib_alloc_default();
    return p;
}
int mulib_strpool_reserve(mulib_strpool_t *p, size_t count) {
    size_t capacity;
    mulib_strpool_entry_ **buckets;
    if (!p) return 0;
    if (count <= p->capacity - p->capacity / 4) return 1;
    capacity = p->capacity ? p->capacity : 16;
    while (count > capacity - capacity / 4) {
        if (capacity > SIZE_MAX / 2) return 0;
        capacity *= 2;
    }
    buckets = (mulib_strpool_entry_ **)mulib_alloc_calloc(&p->allocator, capacity, sizeof(*buckets));
    if (!buckets) return 0;
    for (size_t i = 0; i < p->capacity; ++i) {
        mulib_strpool_entry_ *e = p->buckets[i];
        while (e) {
            mulib_strpool_entry_ *next = e->next;
            size_t index = (size_t)e->hash & (capacity - 1);
            e->next = buckets[index];
            buckets[index] = e;
            e = next;
        }
    }
    mulib_alloc_free(&p->allocator, p->buckets);
    p->buckets = buckets;
    p->capacity = capacity;
    return 1;
}
static mulib_strpool_entry_ *mulib_strpool_find_(const mulib_strpool_t *p,
                                                 const char *s, size_t n, uint64_t hash) {
    mulib_strpool_entry_ *e;
    if (!p->capacity) return NULL;
    for (e = p->buckets[(size_t)hash & (p->capacity - 1)]; e; e = e->next)
        if (e->hash == hash && e->length == n && (!n || !memcmp(e->text, s, n))) return e;
    return NULL;
}
const char *mulib_strpool_find_n(const mulib_strpool_t *p, const char *s, size_t n) {
    mulib_strpool_entry_ *e;
    if (!p || (!s && n)) return NULL;
    e = mulib_strpool_find_(p, s, n, mulib_hash_fnv1a(s, n));
    return e ? e->text : NULL;
}
const char *mulib_strpool_intern_n(mulib_strpool_t *p, const char *s, size_t n) {
    uint64_t hash;
    mulib_strpool_entry_ *e;
    size_t index;
    if (!p || (!s && n) || n == SIZE_MAX) return NULL;
    hash = mulib_hash_fnv1a(s, n);
    e = mulib_strpool_find_(p, s, n, hash);
    if (e) return e->text;
    if (p->count == SIZE_MAX || n > SIZE_MAX - p->bytes) return NULL;
    e = (mulib_strpool_entry_ *)mulib_alloc(&p->allocator, sizeof(*e));
    if (!e) return NULL;
    e->text = mulib_alloc_strdup_n(&p->allocator, s, n);
    if (!e->text || !mulib_strpool_reserve(p, p->count + 1)) {
        mulib_alloc_free(&p->allocator, e->text);
        mulib_alloc_free(&p->allocator, e);
        return NULL;
    }
    index = (size_t)hash & (p->capacity - 1);
    e->hash = hash;
    e->length = n;
    e->next = p->buckets[index];
    p->buckets[index] = e;
    ++p->count;
    p->bytes += n;
    return e->text;
}
const char *mulib_strpool_intern(mulib_strpool_t *p, const char *s) {
    return s ? mulib_strpool_intern_n(p, s, strlen(s)) : NULL;
}
size_t mulib_strpool_size(const mulib_strpool_t *p) { return p ? p->count : 0; }
size_t mulib_strpool_bytes(const mulib_strpool_t *p) { return p ? p->bytes : 0; }
void mulib_strpool_reset(mulib_strpool_t *p) {
    if (!p) return;
    for (size_t i = 0; i < p->capacity; ++i) {
        mulib_strpool_entry_ *e = p->buckets[i];
        while (e) {
            mulib_strpool_entry_ *next = e->next;
            mulib_alloc_free(&p->allocator, e->text);
            mulib_alloc_free(&p->allocator, e);
            e = next;
        }
        p->buckets[i] = NULL;
    }
    p->count = p->bytes = 0;
}
void mulib_strpool_destroy(mulib_strpool_t *p) {
    mulib_allocator_t a;
    if (!p) return;
    a = p->allocator;
    mulib_strpool_reset(p);
    mulib_alloc_free(&a, p->buckets);
    mulib_alloc_free(&a, p);
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_STRPOOL_IMPLEMENTED */
