/**
 * @file mulib-avl.h
 * @brief AVL-balanced int64_t to uint64_t ordered map (ISO C11).
 * Define MULIB_IMPL_AVL in one translation unit. No module dependencies.
 * Nodes are owned by the map; values are copied. Callbacks must not mutate it.
 */
#ifndef MULIB_AVL_H
#define MULIB_AVL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_avl_node mulib_avl_node_t;
/** @brief Initialize before use; zero initialization is also valid. */
typedef struct mulib_avl { mulib_avl_node_t *root; size_t size; } mulib_avl_t;
/** @brief Return nonzero to continue ordered traversal, zero to stop. */
typedef int (*mulib_avl_visit_fn)(int64_t key, uint64_t value, void *context);
/** @brief Initialize an uninitialized map (does not release existing nodes). */
void mulib_avl_init(mulib_avl_t *tree);
/** @brief Insert or replace. Return 1 on success, 0 on invalid input/OOM/overflow.
 * Failure leaves the map unchanged; replacement does not change size. */
int mulib_avl_set(mulib_avl_t *tree, int64_t key, uint64_t value);
/** @brief Find a key. Output is optional and unchanged on a miss. */
int mulib_avl_get(const mulib_avl_t *tree, int64_t key, uint64_t *value);
/** @brief Remove a key without allocation; optional output receives its value. */
int mulib_avl_remove(mulib_avl_t *tree, int64_t key, uint64_t *old_value);
/** @brief Visit ascending keys; return the number of callbacks, including stop. */
size_t mulib_avl_foreach(const mulib_avl_t *tree, mulib_avl_visit_fn visit, void *context);
/** @brief Check ordering, heights, balance factors and size of a live map. */
int mulib_avl_validate(const mulib_avl_t *tree);
/** @brief Release nodes and reset the map; NULL is accepted. */
void mulib_avl_free(mulib_avl_t *tree);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_AVL_H */

#if defined(MULIB_IMPL_AVL) && !defined(MULIB_AVL_IMPLEMENTED)
#define MULIB_AVL_IMPLEMENTED
#include <stdlib.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_avl_node {
    struct mulib_avl_node *left, *right;
    int64_t key;
    uint64_t value;
    int height;
};
static int mulib_avl_height_(const mulib_avl_node_t *n) { return n ? n->height : 0; }
static void mulib_avl_update_(mulib_avl_node_t *n) {
    int a = mulib_avl_height_(n->left), b = mulib_avl_height_(n->right);
    n->height = 1 + (a > b ? a : b);
}
static mulib_avl_node_t *mulib_avl_rotate_left_(mulib_avl_node_t *n) {
    mulib_avl_node_t *r = n->right;
    n->right = r->left; r->left = n;
    mulib_avl_update_(n); mulib_avl_update_(r); return r;
}
static mulib_avl_node_t *mulib_avl_rotate_right_(mulib_avl_node_t *n) {
    mulib_avl_node_t *l = n->left;
    n->left = l->right; l->right = n;
    mulib_avl_update_(n); mulib_avl_update_(l); return l;
}
static mulib_avl_node_t *mulib_avl_balance_(mulib_avl_node_t *n) {
    int d;
    if (!n) return NULL;
    mulib_avl_update_(n);
    d = mulib_avl_height_(n->left) - mulib_avl_height_(n->right);
    if (d > 1) {
        if (mulib_avl_height_(n->left->left) < mulib_avl_height_(n->left->right))
            n->left = mulib_avl_rotate_left_(n->left);
        return mulib_avl_rotate_right_(n);
    }
    if (d < -1) {
        if (mulib_avl_height_(n->right->right) < mulib_avl_height_(n->right->left))
            n->right = mulib_avl_rotate_right_(n->right);
        return mulib_avl_rotate_left_(n);
    }
    return n;
}
void mulib_avl_init(mulib_avl_t *t) { if (t) { t->root = NULL; t->size = 0; } }
int mulib_avl_get(const mulib_avl_t *t, int64_t k, uint64_t *out) {
    mulib_avl_node_t *n;
    if (!t) return 0;
    n = t->root;
    while (n) {
        if (k == n->key) { if (out) *out = n->value; return 1; }
        n = k < n->key ? n->left : n->right;
    }
    return 0;
}
static mulib_avl_node_t *mulib_avl_insert_(mulib_avl_node_t *n, mulib_avl_node_t *e) {
    if (!n) return e;
    if (e->key < n->key) n->left = mulib_avl_insert_(n->left, e);
    else n->right = mulib_avl_insert_(n->right, e);
    return mulib_avl_balance_(n);
}
int mulib_avl_set(mulib_avl_t *t, int64_t k, uint64_t v) {
    mulib_avl_node_t *n, *e;
    if (!t) return 0;
    n = t->root;
    while (n) {
        if (k == n->key) { n->value = v; return 1; }
        n = k < n->key ? n->left : n->right;
    }
    if (t->size == SIZE_MAX) return 0;
    e = (mulib_avl_node_t *)malloc(sizeof(*e));
    if (!e) return 0;
    e->left = e->right = NULL; e->key = k; e->value = v; e->height = 1;
    t->root = mulib_avl_insert_(t->root, e); ++t->size; return 1;
}
static mulib_avl_node_t *mulib_avl_erase_(mulib_avl_node_t *n, int64_t k) {
    if (k < n->key) n->left = mulib_avl_erase_(n->left, k);
    else if (k > n->key) n->right = mulib_avl_erase_(n->right, k);
    else {
        mulib_avl_node_t *s;
        if (!n->left || !n->right) {
            s = n->left ? n->left : n->right; free(n); return s;
        }
        s = n->right;
        while (s->left) s = s->left;
        n->key = s->key; n->value = s->value;
        n->right = mulib_avl_erase_(n->right, s->key);
    }
    return mulib_avl_balance_(n);
}
int mulib_avl_remove(mulib_avl_t *t, int64_t k, uint64_t *out) {
    uint64_t v;
    if (!mulib_avl_get(t, k, &v)) return 0;
    t->root = mulib_avl_erase_(t->root, k); --t->size;
    if (out) *out = v;
    return 1;
}
static int mulib_avl_walk_(const mulib_avl_node_t *n, mulib_avl_visit_fn fn,
                           void *ctx, size_t *count) {
    if (!n) return 1;
    if (!mulib_avl_walk_(n->left, fn, ctx, count)) return 0;
    ++*count;
    if (!fn(n->key, n->value, ctx)) return 0;
    return mulib_avl_walk_(n->right, fn, ctx, count);
}
size_t mulib_avl_foreach(const mulib_avl_t *t, mulib_avl_visit_fn fn, void *ctx) {
    size_t n = 0;
    if (t && fn) (void)mulib_avl_walk_(t->root, fn, ctx, &n);
    return n;
}
static int mulib_avl_check_(const mulib_avl_node_t *n, const int64_t *lo,
                            const int64_t *hi, size_t depth, size_t *count) {
    int a, b;
    if (!n) return 1;
    if (depth > 2 * sizeof(size_t) * CHAR_BIT || *count == SIZE_MAX ||
        (lo && n->key <= *lo) || (hi && n->key >= *hi)) return 0;
    ++*count;
    if (!mulib_avl_check_(n->left, lo, &n->key, depth + 1, count) ||
        !mulib_avl_check_(n->right, &n->key, hi, depth + 1, count)) return 0;
    a = mulib_avl_height_(n->left); b = mulib_avl_height_(n->right);
    return a - b >= -1 && a - b <= 1 && n->height == 1 + (a > b ? a : b);
}
int mulib_avl_validate(const mulib_avl_t *t) {
    size_t count = 0;
    return t && mulib_avl_check_(t->root, NULL, NULL, 0, &count) && count == t->size;
}
static void mulib_avl_destroy_(mulib_avl_node_t *n) {
    if (n) { mulib_avl_destroy_(n->left); mulib_avl_destroy_(n->right); free(n); }
}
void mulib_avl_free(mulib_avl_t *t) {
    if (t) { mulib_avl_destroy_(t->root); mulib_avl_init(t); }
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
