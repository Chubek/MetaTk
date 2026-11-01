/**
 * @file mulib-alloc.h
 * @brief Pluggable allocators and checked, transactional array resizing.
 * Define MULIB_IMPL_ALLOC and MULIB_IMPL_PORT once. Allocator callbacks must
 * provide malloc-compatible alignment and realloc-compatible failure semantics.
 */
#ifndef MULIB_ALLOC_H
#define MULIB_ALLOC_H
#include <stddef.h>
#include "mulib-port.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef void *(*mulib_alloc_allocate_fn)(void *context, size_t size);
typedef void *(*mulib_alloc_reallocate_fn)(void *context, void *memory, size_t size);
typedef void (*mulib_alloc_deallocate_fn)(void *context, void *memory);
/** @brief Callbacks are borrowed; the allocator struct itself may be copied. */
typedef struct mulib_allocator {
    void *context;
    mulib_alloc_allocate_fn allocate;
    mulib_alloc_reallocate_fn reallocate;
    mulib_alloc_deallocate_fn deallocate;
} mulib_allocator_t;
/** @brief Return a value object backed by malloc/realloc/free. */
mulib_allocator_t mulib_alloc_default(void);
/** @brief Require allocate and deallocate; reallocate may be NULL. */
int mulib_alloc_valid(const mulib_allocator_t *allocator);
/** @brief Allocate bytes, or NULL for zero size/failure; NULL allocator is libc. */
void *mulib_alloc(const mulib_allocator_t *allocator, size_t size);
/** @brief Allocate and zero count*stride bytes with overflow checking. */
void *mulib_alloc_calloc(const mulib_allocator_t *allocator, size_t count, size_t stride);
/** @brief Resize; size zero frees, failure preserves memory. */
void *mulib_alloc_realloc(const mulib_allocator_t *allocator, void *memory, size_t size);
/** @brief Free a pointer obtained from this allocator; NULL is harmless. */
void mulib_alloc_free(const mulib_allocator_t *allocator, void *memory);
/** @brief Resize *memory transactionally; count zero frees and NULLs it. */
int mulib_alloc_resize_array(const mulib_allocator_t *allocator, void **memory,
                             size_t count, size_t stride);
/** @brief Return an owned, NUL-terminated copy of length-delimited bytes. */
char *mulib_alloc_strdup_n(const mulib_allocator_t *allocator, const char *text,
                          size_t length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_ALLOC_H */

#if defined(MULIB_IMPL_ALLOC) && !defined(MULIB_ALLOC_IMPLEMENTED)
#define MULIB_ALLOC_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static void *mulib_alloc_libc_allocate_(void *context, size_t n) {
    (void)context;
    return malloc(n);
}
static void *mulib_alloc_libc_reallocate_(void *context, void *p, size_t n) {
    (void)context;
    return realloc(p, n);
}
static void mulib_alloc_libc_deallocate_(void *context, void *p) {
    (void)context;
    free(p);
}
mulib_allocator_t mulib_alloc_default(void) {
    mulib_allocator_t a;
    a.context = NULL;
    a.allocate = mulib_alloc_libc_allocate_;
    a.reallocate = mulib_alloc_libc_reallocate_;
    a.deallocate = mulib_alloc_libc_deallocate_;
    return a;
}
int mulib_alloc_valid(const mulib_allocator_t *a) {
    return !a || (a->allocate && a->deallocate);
}
void *mulib_alloc(const mulib_allocator_t *a, size_t n) {
    if (!n || !mulib_alloc_valid(a)) return NULL;
    return a ? a->allocate(a->context, n) : malloc(n);
}
void *mulib_alloc_calloc(const mulib_allocator_t *a, size_t count, size_t stride) {
    size_t n;
    void *p;
    if (!mulib_port_mul_size(count, stride, &n) || !n) return NULL;
    p = mulib_alloc(a, n);
    if (p) memset(p, 0, n);
    return p;
}
void mulib_alloc_free(const mulib_allocator_t *a, void *p) {
    if (!p || !mulib_alloc_valid(a)) return;
    if (a) a->deallocate(a->context, p);
    else free(p);
}
void *mulib_alloc_realloc(const mulib_allocator_t *a, void *p, size_t n) {
    if (!mulib_alloc_valid(a)) return NULL;
    if (!n) { mulib_alloc_free(a, p); return NULL; }
    if (!p) return mulib_alloc(a, n);
    if (a) return a->reallocate ? a->reallocate(a->context, p, n) : NULL;
    return realloc(p, n);
}
int mulib_alloc_resize_array(const mulib_allocator_t *a, void **p,
                             size_t count, size_t stride) {
    size_t n;
    void *next;
    if (!p || !mulib_alloc_valid(a) || (count && !stride) ||
        !mulib_port_mul_size(count, stride, &n)) return 0;
    if (!n) { mulib_alloc_free(a, *p); *p = NULL; return 1; }
    next = mulib_alloc_realloc(a, *p, n);
    if (!next) return 0;
    *p = next;
    return 1;
}
char *mulib_alloc_strdup_n(const mulib_allocator_t *a, const char *s, size_t n) {
    size_t bytes;
    char *p;
    if ((!s && n) || !mulib_port_add_size(n, 1, &bytes)) return NULL;
    p = (char *)mulib_alloc(a, bytes);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_ALLOC_IMPLEMENTED */
