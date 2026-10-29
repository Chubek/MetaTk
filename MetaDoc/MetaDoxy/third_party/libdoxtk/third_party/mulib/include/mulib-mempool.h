/**
 * @file mulib-mempool.h
 * @brief Growing fixed-size pool with aligned stable slots and checked release.
 * Define MULIB_IMPL_MEMPOOL, MULIB_IMPL_ALLOC, MULIB_IMPL_PORT once. Slots are
 * aligned to max_align_t; over-aligned types are unsupported. Not thread-safe.
 */
#ifndef MULIB_MEMPOOL_H
#define MULIB_MEMPOOL_H
#include "mulib-alloc.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_mempool mulib_mempool_t;
/** @brief Create a pool; nonzero sizes required, allocator is copied. */
mulib_mempool_t *mulib_mempool_create(size_t object_size, size_t slots_per_chunk,
                                    const mulib_allocator_t *allocator);
/** @brief Get an uninitialized slot, growing on demand. */
void *mulib_mempool_acquire(mulib_mempool_t *pool);
/** @brief Return an allocated slot; reject foreign, interior, or double frees. */
int mulib_mempool_release(mulib_mempool_t *pool, void *slot);
/** @brief Return the number of currently allocated slots. */
size_t mulib_mempool_in_use(const mulib_mempool_t *pool);
/** @brief Return total slots across allocated chunks. */
size_t mulib_mempool_capacity(const mulib_mempool_t *pool);
/** @brief Invalidate all slots without releasing chunks. */
void mulib_mempool_reset(mulib_mempool_t *pool);
/** @brief Invalidate slots and release all chunks, retaining the pool object. */
void mulib_mempool_clear(mulib_mempool_t *pool);
/** @brief Release chunks and the pool; NULL is harmless. */
void mulib_mempool_destroy(mulib_mempool_t *pool);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MEMPOOL_H */

#if defined(MULIB_IMPL_MEMPOOL) && !defined(MULIB_MEMPOOL_IMPLEMENTED)
#define MULIB_MEMPOOL_IMPLEMENTED
#include <stdint.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_mempool_slot_ {
    struct mulib_mempool_slot_ *next;
} mulib_mempool_slot_;
typedef struct mulib_mempool_chunk_ {
    struct mulib_mempool_chunk_ *next;
    unsigned char *data, *used;
} mulib_mempool_chunk_;
struct mulib_mempool {
    mulib_allocator_t allocator;
    mulib_mempool_chunk_ *chunks;
    mulib_mempool_slot_ *available;
    size_t stride, slots, bytes, capacity, in_use;
};
static mulib_mempool_chunk_ *mulib_mempool_find_(mulib_mempool_t *p,
                                                const void *slot, size_t *index) {
    uintptr_t address = (uintptr_t)slot;
    mulib_mempool_chunk_ *c;
    for (c = p->chunks; c; c = c->next) {
        uintptr_t base = (uintptr_t)c->data;
        if (address >= base && address - base < p->bytes &&
            (address - base) % p->stride == 0) {
            *index = (size_t)(address - base) / p->stride;
            return c;
        }
    }
    return NULL;
}
static int mulib_mempool_grow_(mulib_mempool_t *p) {
    mulib_mempool_chunk_ *c;
    if (p->slots > SIZE_MAX - p->capacity) return 0;
    c = (mulib_mempool_chunk_ *)mulib_alloc(&p->allocator, sizeof(*c));
    if (!c) return 0;
    c->data = (unsigned char *)mulib_alloc(&p->allocator, p->bytes);
    c->used = (unsigned char *)mulib_alloc_calloc(&p->allocator, (p->slots + 7) / 8, 1);
    if (!c->data || !c->used) {
        mulib_alloc_free(&p->allocator, c->data);
        mulib_alloc_free(&p->allocator, c->used);
        mulib_alloc_free(&p->allocator, c);
        return 0;
    }
    c->next = p->chunks;
    p->chunks = c;
    for (size_t i = p->slots; i; --i) {
        mulib_mempool_slot_ *s = (mulib_mempool_slot_ *)(void *)(c->data + (i - 1) * p->stride);
        s->next = p->available;
        p->available = s;
    }
    p->capacity += p->slots;
    return 1;
}
mulib_mempool_t *mulib_mempool_create(size_t size, size_t slots,
                                    const mulib_allocator_t *a) {
    size_t stride, bytes;
    mulib_mempool_t *p;
    if (!size || !slots || slots > SIZE_MAX - 7 || !mulib_alloc_valid(a)) return NULL;
    if (size < sizeof(mulib_mempool_slot_)) size = sizeof(mulib_mempool_slot_);
    if (!mulib_port_align_size(size, _Alignof(max_align_t), &stride) ||
        !mulib_port_mul_size(stride, slots, &bytes)) return NULL;
    p = (mulib_mempool_t *)mulib_alloc_calloc(a, 1, sizeof(*p));
    if (!p) return NULL;
    p->allocator = a ? *a : mulib_alloc_default();
    p->stride = stride;
    p->slots = slots;
    p->bytes = bytes;
    return p;
}
void *mulib_mempool_acquire(mulib_mempool_t *p) {
    mulib_mempool_slot_ *s;
    mulib_mempool_chunk_ *c;
    size_t i;
    if (!p || (!p->available && !mulib_mempool_grow_(p))) return NULL;
    s = p->available;
    c = mulib_mempool_find_(p, s, &i);
    if (!c) return NULL;
    p->available = s->next;
    c->used[i / 8] |= (unsigned char)(1u << (i % 8));
    ++p->in_use;
    return s;
}
int mulib_mempool_release(mulib_mempool_t *p, void *slot) {
    mulib_mempool_chunk_ *c;
    mulib_mempool_slot_ *s;
    size_t i;
    if (!p || !slot || !(c = mulib_mempool_find_(p, slot, &i)) ||
        !(c->used[i / 8] & (1u << (i % 8)))) return 0;
    c->used[i / 8] &= (unsigned char)~(1u << (i % 8));
    s = (mulib_mempool_slot_ *)slot;
    s->next = p->available;
    p->available = s;
    --p->in_use;
    return 1;
}
size_t mulib_mempool_in_use(const mulib_mempool_t *p) { return p ? p->in_use : 0; }
size_t mulib_mempool_capacity(const mulib_mempool_t *p) { return p ? p->capacity : 0; }
void mulib_mempool_reset(mulib_mempool_t *p) {
    mulib_mempool_chunk_ *c;
    if (!p) return;
    p->available = NULL;
    p->in_use = 0;
    for (c = p->chunks; c; c = c->next) {
        memset(c->used, 0, (p->slots + 7) / 8);
        for (size_t i = p->slots; i; --i) {
            mulib_mempool_slot_ *s = (mulib_mempool_slot_ *)(void *)(c->data + (i - 1) * p->stride);
            s->next = p->available;
            p->available = s;
        }
    }
}
void mulib_mempool_clear(mulib_mempool_t *p) {
    if (!p) return;
    while (p->chunks) {
        mulib_mempool_chunk_ *c = p->chunks;
        p->chunks = c->next;
        mulib_alloc_free(&p->allocator, c->data);
        mulib_alloc_free(&p->allocator, c->used);
        mulib_alloc_free(&p->allocator, c);
    }
    p->available = NULL;
    p->capacity = p->in_use = 0;
}
void mulib_mempool_destroy(mulib_mempool_t *p) {
    mulib_allocator_t a;
    if (!p) return;
    a = p->allocator;
    mulib_mempool_clear(p);
    mulib_alloc_free(&a, p);
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MEMPOOL_IMPLEMENTED */
