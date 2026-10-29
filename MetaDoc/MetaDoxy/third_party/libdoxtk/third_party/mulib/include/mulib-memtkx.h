/**
 * @file mulib-memtkx.h
 * @brief Tracking allocator with live-byte limits, statistics, and checkpoints.
 * This original memory-toolkit extension is not an implementation of any
 * external memtkx API. Define MULIB_IMPL_MEMTKX, MULIB_IMPL_ALLOC,
 * MULIB_IMPL_PORT once. Not thread-safe; no global allocator is replaced.
 */
#ifndef MULIB_MEMTKX_H
#define MULIB_MEMTKX_H
#include <stdint.h>
#include "mulib-alloc.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_memtkx mulib_memtkx_t;
typedef struct mulib_memtkx_stats {
    size_t live_blocks, live_bytes, peak_bytes;
    size_t allocations, reallocations, failures;
} mulib_memtkx_stats_t;
/** @brief Create a tracker using a copied backing allocator. */
mulib_memtkx_t *mulib_memtkx_create(const mulib_allocator_t *backing);
/** @brief Borrow an allocator view; it is valid only while tracker lives. */
mulib_allocator_t mulib_memtkx_allocator(mulib_memtkx_t *tracker);
/** @brief Get counters; cumulative counters saturate instead of wrapping. */
mulib_memtkx_stats_t mulib_memtkx_stats(const mulib_memtkx_t *tracker);
/** @brief Set maximum live payload bytes; fail if already above that limit. */
int mulib_memtkx_set_limit(mulib_memtkx_t *tracker, size_t limit);
/** @brief Return a monotonically increasing allocation checkpoint. */
uint64_t mulib_memtkx_mark(const mulib_memtkx_t *tracker);
/** @brief Release allocations made after mark; resizing retains original mark. */
size_t mulib_memtkx_release_since(mulib_memtkx_t *tracker, uint64_t mark);
/** @brief Release all tracked allocations; preserve cumulative statistics. */
void mulib_memtkx_reset(mulib_memtkx_t *tracker);
/** @brief Release outstanding allocations and the tracker itself. */
void mulib_memtkx_destroy(mulib_memtkx_t *tracker);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MEMTKX_H */

#if defined(MULIB_IMPL_MEMTKX) && !defined(MULIB_MEMTKX_IMPLEMENTED)
#define MULIB_MEMTKX_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_memtkx_block_ {
    struct mulib_memtkx_block_ *previous, *next;
    size_t size;
    uint64_t sequence;
    max_align_t alignment;
} mulib_memtkx_block_;
struct mulib_memtkx {
    mulib_allocator_t backing;
    mulib_memtkx_block_ *head;
    mulib_memtkx_stats_t stats;
    size_t limit;
    uint64_t sequence;
};
static void mulib_memtkx_increment_(size_t *n) { if (*n != SIZE_MAX) ++*n; }
static mulib_memtkx_block_ *mulib_memtkx_find_(mulib_memtkx_t *t, const void *p) {
    mulib_memtkx_block_ *b;
    for (b = t->head; b; b = b->next) if ((void *)(b + 1) == p) return b;
    return NULL;
}
static void mulib_memtkx_unlink_(mulib_memtkx_t *t, mulib_memtkx_block_ *b) {
    if (b->previous) b->previous->next = b->next;
    else t->head = b->next;
    if (b->next) b->next->previous = b->previous;
    t->stats.live_bytes -= b->size;
    --t->stats.live_blocks;
    mulib_alloc_free(&t->backing, b);
}
static void *mulib_memtkx_allocate_(void *context, size_t n) {
    mulib_memtkx_t *t = (mulib_memtkx_t *)context;
    mulib_memtkx_block_ *b;
    size_t total;
    if (!t || !n) return NULL;
    if (n > t->limit - t->stats.live_bytes || t->sequence == UINT64_MAX ||
        t->stats.live_blocks == SIZE_MAX || !mulib_port_add_size(sizeof(*b), n, &total)) {
        mulib_memtkx_increment_(&t->stats.failures);
        return NULL;
    }
    b = (mulib_memtkx_block_ *)mulib_alloc(&t->backing, total);
    if (!b) { mulib_memtkx_increment_(&t->stats.failures); return NULL; }
    b->previous = NULL;
    b->next = t->head;
    if (b->next) b->next->previous = b;
    b->size = n;
    b->sequence = ++t->sequence;
    t->head = b;
    ++t->stats.live_blocks;
    t->stats.live_bytes += n;
    if (t->stats.live_bytes > t->stats.peak_bytes) t->stats.peak_bytes = t->stats.live_bytes;
    mulib_memtkx_increment_(&t->stats.allocations);
    return b + 1;
}
static void mulib_memtkx_deallocate_(void *context, void *p) {
    mulib_memtkx_t *t = (mulib_memtkx_t *)context;
    mulib_memtkx_block_ *b;
    if (!t || !p) return;
    b = mulib_memtkx_find_(t, p);
    if (!b) { mulib_memtkx_increment_(&t->stats.failures); return; }
    mulib_memtkx_unlink_(t, b);
}
static void *mulib_memtkx_reallocate_(void *context, void *p, size_t n) {
    mulib_memtkx_t *t = (mulib_memtkx_t *)context;
    mulib_memtkx_block_ *b, *next;
    size_t total, old_size;
    if (!t) return NULL;
    if (!p) return mulib_memtkx_allocate_(context, n);
    if (!n) { mulib_memtkx_deallocate_(context, p); return NULL; }
    b = mulib_memtkx_find_(t, p);
    if (!b || n > t->limit - (t->stats.live_bytes - b->size) ||
        !mulib_port_add_size(sizeof(*b), n, &total)) {
        mulib_memtkx_increment_(&t->stats.failures);
        return NULL;
    }
    old_size = b->size;
    if (t->backing.reallocate) next = (mulib_memtkx_block_ *)mulib_alloc_realloc(&t->backing, b, total);
    else {
        next = (mulib_memtkx_block_ *)mulib_alloc(&t->backing, total);
        if (next) {
            memcpy(next, b, sizeof(*b));
            memcpy(next + 1, b + 1, old_size < n ? old_size : n);
            mulib_alloc_free(&t->backing, b);
        }
    }
    if (!next) { mulib_memtkx_increment_(&t->stats.failures); return NULL; }
    if (next->previous) next->previous->next = next;
    else t->head = next;
    if (next->next) next->next->previous = next;
    next->size = n;
    t->stats.live_bytes = t->stats.live_bytes - old_size + n;
    if (t->stats.live_bytes > t->stats.peak_bytes) t->stats.peak_bytes = t->stats.live_bytes;
    mulib_memtkx_increment_(&t->stats.reallocations);
    return next + 1;
}
mulib_memtkx_t *mulib_memtkx_create(const mulib_allocator_t *a) {
    mulib_memtkx_t *t;
    if (!mulib_alloc_valid(a)) return NULL;
    t = (mulib_memtkx_t *)mulib_alloc_calloc(a, 1, sizeof(*t));
    if (!t) return NULL;
    t->backing = a ? *a : mulib_alloc_default();
    t->limit = SIZE_MAX;
    return t;
}
mulib_allocator_t mulib_memtkx_allocator(mulib_memtkx_t *t) {
    mulib_allocator_t a;
    a.context = t;
    a.allocate = mulib_memtkx_allocate_;
    a.reallocate = mulib_memtkx_reallocate_;
    a.deallocate = mulib_memtkx_deallocate_;
    return a;
}
mulib_memtkx_stats_t mulib_memtkx_stats(const mulib_memtkx_t *t) {
    mulib_memtkx_stats_t empty = {0, 0, 0, 0, 0, 0};
    return t ? t->stats : empty;
}
int mulib_memtkx_set_limit(mulib_memtkx_t *t, size_t n) {
    if (!t || n < t->stats.live_bytes) return 0;
    t->limit = n;
    return 1;
}
uint64_t mulib_memtkx_mark(const mulib_memtkx_t *t) { return t ? t->sequence : 0; }
size_t mulib_memtkx_release_since(mulib_memtkx_t *t, uint64_t mark) {
    size_t count = 0;
    mulib_memtkx_block_ *b;
    if (!t) return 0;
    b = t->head;
    while (b) {
        mulib_memtkx_block_ *next = b->next;
        if (b->sequence > mark) { mulib_memtkx_unlink_(t, b); ++count; }
        b = next;
    }
    return count;
}
void mulib_memtkx_reset(mulib_memtkx_t *t) { (void)mulib_memtkx_release_since(t, 0); }
void mulib_memtkx_destroy(mulib_memtkx_t *t) {
    mulib_allocator_t a;
    if (!t) return;
    a = t->backing;
    mulib_memtkx_reset(t);
    mulib_alloc_free(&a, t);
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MEMTKX_IMPLEMENTED */
