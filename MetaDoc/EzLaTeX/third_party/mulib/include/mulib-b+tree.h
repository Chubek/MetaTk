/**
 * @file mulib-b+tree.h
 * @brief Order-eight B+ tree: int64_t keys, uint64_t values, linked leaves.
 * Define MULIB_IMPL_BPTREE in one translation unit; no module dependencies.
 * Internal nodes route by child minima; all records reside in sorted leaves.
 */
#ifndef MULIB_BPTREE_H
#define MULIB_BPTREE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_bptree_node mulib_bptree_node_t;
  /** @brief Owning tree; initialize before use or use a zero initializer. */
  typedef struct mulib_bptree
  {
    mulib_bptree_node_t *root, *first;
    size_t size;
  } mulib_bptree_t;
  /** @brief Return nonzero to continue, zero to stop; do not mutate the tree.
   */
  typedef int (*mulib_bptree_visit_fn) (int64_t key, uint64_t value,
                                        void *context);
  /** @brief Initialize an uninitialized tree. */
  void mulib_bptree_init (mulib_bptree_t *tree);
  /** @brief Insert/replace; 1 succeeds, 0 means invalid input/OOM/overflow.
   * All splitting allocations precede mutation, so failure leaves it
   * unchanged. */
  int mulib_bptree_set (mulib_bptree_t *tree, int64_t key, uint64_t value);
  /** @brief Find a record; optional output is unchanged on a miss. */
  int mulib_bptree_get (const mulib_bptree_t *tree, int64_t key,
                        uint64_t *value);
  /** @brief Remove with borrowing/merging, without allocation; return 1 if
   * found. */
  int mulib_bptree_remove (mulib_bptree_t *tree, int64_t key,
                           uint64_t *old_value);
  /** @brief Visit inclusive [low,high] in order via linked leaves; count
   * callbacks. */
  size_t mulib_bptree_range (const mulib_bptree_t *tree, int64_t low,
                             int64_t high, mulib_bptree_visit_fn visit,
                             void *context);
  /** @brief Verify occupancy, parent links, separators, depths, leaf chain and
   * size. */
  int mulib_bptree_validate (const mulib_bptree_t *tree);
  /** @brief Release all storage and reset; NULL is accepted. */
  void mulib_bptree_free (mulib_bptree_t *tree);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BPTREE_H */

#if defined(MULIB_IMPL_BPTREE) && !defined(MULIB_BPTREE_IMPLEMENTED)
#define MULIB_BPTREE_IMPLEMENTED
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  struct mulib_bptree_node
  {
    struct mulib_bptree_node *parent, *next, *prev;
    size_t n;
    int leaf;
    int64_t keys[9];
    uint64_t values[9];
    struct mulib_bptree_node *child[9];
  };
  void
  mulib_bptree_init (mulib_bptree_t *t)
  {
    if (t)
      {
        t->root = t->first = NULL;
        t->size = 0;
      }
  }
  static mulib_bptree_node_t *
  mulib_bptree_leaf_ (const mulib_bptree_t *t, int64_t k)
  {
    mulib_bptree_node_t *n = t->root;
    while (n && !n->leaf)
      {
        size_t i = 1;
        while (i < n->n && k >= n->keys[i])
          ++i;
        n = n->child[i - 1];
      }
    return n;
  }
  static size_t
  mulib_bptree_pos_ (const mulib_bptree_node_t *n, int64_t k)
  {
    size_t lo = 0, hi = n->n;
    while (lo < hi)
      {
        size_t m = lo + (hi - lo) / 2;
        if (n->keys[m] < k)
          lo = m + 1;
        else
          hi = m;
      }
    return lo;
  }
  static void
  mulib_bptree_refresh_ (mulib_bptree_node_t *n)
  {
    size_t i;
    if (!n->leaf)
      for (i = 0; i < n->n; ++i)
        n->keys[i] = n->child[i]->keys[0];
  }
  static void
  mulib_bptree_up_ (mulib_bptree_node_t *n)
  {
    while (n)
      {
        mulib_bptree_refresh_ (n);
        n = n->parent;
      }
  }
  int
  mulib_bptree_get (const mulib_bptree_t *t, int64_t k, uint64_t *out)
  {
    mulib_bptree_node_t *n;
    size_t i;
    if (!t || !(n = mulib_bptree_leaf_ (t, k)))
      return 0;
    i = mulib_bptree_pos_ (n, k);
    if (i == n->n || n->keys[i] != k)
      return 0;
    if (out)
      *out = n->values[i];
    return 1;
  }
  static mulib_bptree_node_t *
  mulib_bptree_take_ (mulib_bptree_node_t **pool)
  {
    mulib_bptree_node_t *n = *pool;
    *pool = n->next;
    n->next = NULL;
    return n;
  }
  int
  mulib_bptree_set (mulib_bptree_t *t, int64_t k, uint64_t v)
  {
    mulib_bptree_node_t *n, *p, *pool = NULL;
    size_t i, needed = 0;
    if (!t)
      return 0;
    if (!t->root)
      {
        n = (mulib_bptree_node_t *)calloc (1, sizeof (*n));
        if (!n)
          return 0;
        n->leaf = 1;
        n->n = 1;
        n->keys[0] = k;
        n->values[0] = v;
        t->root = t->first = n;
        t->size = 1;
        return 1;
      }
    n = mulib_bptree_leaf_ (t, k);
    i = mulib_bptree_pos_ (n, k);
    if (i < n->n && n->keys[i] == k)
      {
        n->values[i] = v;
        return 1;
      }
    if (t->size == SIZE_MAX)
      return 0;
    p = n;
    while (p && p->n == 8)
      {
        ++needed;
        p = p->parent;
      }
    if (needed && !p)
      ++needed;
    while (needed--)
      {
        p = (mulib_bptree_node_t *)calloc (1, sizeof (*p));
        if (!p)
          {
            while (pool)
              {
                p = pool->next;
                free (pool);
                pool = p;
              }
            return 0;
          }
        p->next = pool;
        pool = p;
      }
    memmove (n->keys + i + 1, n->keys + i, (n->n - i) * sizeof (n->keys[0]));
    memmove (n->values + i + 1, n->values + i,
             (n->n - i) * sizeof (n->values[0]));
    n->keys[i] = k;
    n->values[i] = v;
    ++n->n;
    ++t->size;
    while (n->n > 8)
      {
        mulib_bptree_node_t *r = mulib_bptree_take_ (&pool);
        r->leaf = n->leaf;
        r->n = n->n - 4;
        r->parent = n->parent;
        for (i = 0; i < r->n; ++i)
          {
            r->keys[i] = n->keys[i + 4];
            if (n->leaf)
              r->values[i] = n->values[i + 4];
            else
              {
                r->child[i] = n->child[i + 4];
                r->child[i]->parent = r;
              }
          }
        n->n = 4;
        if (n->leaf)
          {
            r->next = n->next;
            r->prev = n;
            if (r->next)
              r->next->prev = r;
            n->next = r;
          }
        p = n->parent;
        if (!p)
          {
            p = mulib_bptree_take_ (&pool);
            p->n = 2;
            p->child[0] = n;
            p->child[1] = r;
            n->parent = r->parent = p;
            t->root = p;
            mulib_bptree_refresh_ (p);
            n = p;
            break;
          }
        i = 0;
        while (p->child[i] != n)
          ++i;
        memmove (p->child + i + 2, p->child + i + 1,
                 (p->n - i - 1) * sizeof (p->child[0]));
        p->child[i + 1] = r;
        ++p->n;
        mulib_bptree_refresh_ (p);
        n = p;
      }
    mulib_bptree_up_ (n);
    return 1;
  }
  static void
  mulib_bptree_drop_item_ (mulib_bptree_node_t *n, size_t i)
  {
    --n->n;
    if (n->leaf)
      {
        memmove (n->keys + i, n->keys + i + 1,
                 (n->n - i) * sizeof (n->keys[0]));
        memmove (n->values + i, n->values + i + 1,
                 (n->n - i) * sizeof (n->values[0]));
      }
    else
      {
        memmove (n->child + i, n->child + i + 1,
                 (n->n - i) * sizeof (n->child[0]));
        mulib_bptree_refresh_ (n);
      }
  }
  int
  mulib_bptree_remove (mulib_bptree_t *t, int64_t k, uint64_t *out)
  {
    mulib_bptree_node_t *n;
    size_t i;
    if (!t || !(n = mulib_bptree_leaf_ (t, k)))
      return 0;
    i = mulib_bptree_pos_ (n, k);
    if (i == n->n || n->keys[i] != k)
      return 0;
    if (out)
      *out = n->values[i];
    mulib_bptree_drop_item_ (n, i);
    --t->size;
    while (n->parent && n->n < 4)
      {
        mulib_bptree_node_t *p = n->parent, *l, *r;
        i = 0;
        while (p->child[i] != n)
          ++i;
        l = i ? p->child[i - 1] : NULL;
        r = i + 1 < p->n ? p->child[i + 1] : NULL;
        if (l && l->n > 4)
          {
            if (n->leaf)
              {
                memmove (n->keys + 1, n->keys, n->n * sizeof (n->keys[0]));
                memmove (n->values + 1, n->values,
                         n->n * sizeof (n->values[0]));
                n->keys[0] = l->keys[l->n - 1];
                n->values[0] = l->values[l->n - 1];
              }
            else
              {
                memmove (n->child + 1, n->child, n->n * sizeof (n->child[0]));
                n->child[0] = l->child[l->n - 1];
                n->child[0]->parent = n;
              }
            --l->n;
            ++n->n;
            mulib_bptree_refresh_ (l);
            mulib_bptree_up_ (n);
            return 1;
          }
        if (r && r->n > 4)
          {
            if (n->leaf)
              {
                n->keys[n->n] = r->keys[0];
                n->values[n->n] = r->values[0];
              }
            else
              {
                n->child[n->n] = r->child[0];
                n->child[n->n]->parent = n;
              }
            ++n->n;
            mulib_bptree_drop_item_ (r, 0);
            mulib_bptree_up_ (n);
            return 1;
          }
        /* Merge into the left node; if absent, merge the right node into n. */
        if (!l)
          {
            l = n;
            n = r;
            ++i;
          }
        for (size_t j = 0; j < n->n; ++j)
          {
            if (l->leaf)
              {
                l->keys[l->n + j] = n->keys[j];
                l->values[l->n + j] = n->values[j];
              }
            else
              {
                l->child[l->n + j] = n->child[j];
                l->child[l->n + j]->parent = l;
              }
          }
        l->n += n->n;
        if (l->leaf)
          {
            l->next = n->next;
            if (n->next)
              n->next->prev = l;
          }
        mulib_bptree_refresh_ (l);
        free (n);
        mulib_bptree_drop_item_ (p, i);
        n = p;
      }
    if (n == t->root)
      {
        if (!n->leaf && n->n == 1)
          {
            t->root = n->child[0];
            t->root->parent = NULL;
            free (n);
            n = t->root;
          }
        else if (n->leaf && !n->n)
          {
            free (n);
            t->root = t->first = NULL;
            return 1;
          }
      }
    mulib_bptree_up_ (n);
    return 1;
  }
  size_t
  mulib_bptree_range (const mulib_bptree_t *t, int64_t lo, int64_t hi,
                      mulib_bptree_visit_fn fn, void *ctx)
  {
    mulib_bptree_node_t *n;
    size_t i, count = 0;
    if (!t || !fn || lo > hi || !(n = mulib_bptree_leaf_ (t, lo)))
      return 0;
    i = mulib_bptree_pos_ (n, lo);
    while (n)
      {
        for (; i < n->n; ++i)
          {
            if (n->keys[i] > hi)
              return count;
            ++count;
            if (!fn (n->keys[i], n->values[i], ctx))
              return count;
          }
        n = n->next;
        i = 0;
      }
    return count;
  }
  typedef struct mulib_bptree_check
  {
    const mulib_bptree_node_t *previous;
    size_t count, leaf_depth;
    int have_depth;
  } mulib_bptree_check_t;
  static int
  mulib_bptree_check_ (const mulib_bptree_node_t *n,
                       const mulib_bptree_node_t *parent, size_t depth,
                       mulib_bptree_check_t *c)
  {
    size_t i;
    if (!n || n->parent != parent || n->n > 8 || !n->n || (parent && n->n < 4)
        || (!parent && !n->leaf && n->n < 2)
        || depth > sizeof (size_t) * CHAR_BIT)
      return 0;
    for (i = 1; i < n->n; ++i)
      if (n->keys[i - 1] >= n->keys[i])
        return 0;
    if (n->leaf)
      {
        if (c->have_depth && c->leaf_depth != depth)
          return 0;
        c->have_depth = 1;
        c->leaf_depth = depth;
        if (n->prev != c->previous
            || (c->previous
                && (c->previous->next != n
                    || c->previous->keys[c->previous->n - 1] >= n->keys[0]))
            || n->n > SIZE_MAX - c->count)
          return 0;
        c->count += n->n;
        c->previous = n;
      }
    else
      {
        for (i = 0; i < n->n; ++i)
          {
            if (!n->child[i] || n->keys[i] != n->child[i]->keys[0]
                || !mulib_bptree_check_ (n->child[i], n, depth + 1, c))
              return 0;
          }
      }
    return 1;
  }
  int
  mulib_bptree_validate (const mulib_bptree_t *t)
  {
    mulib_bptree_check_t c = { NULL, 0, 0, 0 };
    const mulib_bptree_node_t *first;
    if (!t)
      return 0;
    if (!t->root)
      return !t->first && !t->size;
    first = t->root;
    while (!first->leaf)
      {
        if (!first->n || !first->child[0])
          return 0;
        first = first->child[0];
      }
    return first == t->first && mulib_bptree_check_ (t->root, NULL, 0, &c)
           && c.count == t->size && c.previous && !c.previous->next;
  }
  static void
  mulib_bptree_destroy_ (mulib_bptree_node_t *n)
  {
    if (n)
      {
        if (!n->leaf)
          for (size_t i = 0; i < n->n; ++i)
            mulib_bptree_destroy_ (n->child[i]);
        free (n);
      }
  }
  void
  mulib_bptree_free (mulib_bptree_t *t)
  {
    if (t)
      {
        mulib_bptree_destroy_ (t->root);
        mulib_bptree_init (t);
      }
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
