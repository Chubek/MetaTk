/**
 * @file mulib-hattrie.h
 * @brief Focused HAT-style byte-key trie: compressed branches and hashed
 * buckets. Define MULIB_IMPL_HATTRIE and MULIB_IMPL_HASH in one translation
 * unit. Keys (including empty/binary keys) are copied, uint64_t values are
 * copied. This is not a claim of ABI/algorithm compatibility with another HAT
 * library.
 */
#ifndef MULIB_HATTRIE_H
#define MULIB_HATTRIE_H
#include "mulib-hash.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_hattrie_node mulib_hattrie_node_t;
  /** @brief Owns trie nodes and key copies; initialize before use. */
  typedef struct mulib_hattrie
  {
    mulib_hattrie_node_t *root;
    size_t size;
  } mulib_hattrie_t;
  /** @brief Key is borrowed only during the callback; nonzero continues.
   * Enumeration is unordered and callbacks must not mutate the trie. */
  typedef int (*mulib_hattrie_visit_fn) (const void *key, size_t length,
                                         uint64_t value, void *context);
  /** @brief Initialize an uninitialized trie; zero initialization also works.
   */
  void mulib_hattrie_init (mulib_hattrie_t *trie);
  /** @brief Insert/replace; return 1 on success, 0 on invalid
   * input/OOM/overflow. Failure preserves all mappings. A failed optional
   * bucket burst is deferred. */
  int mulib_hattrie_set (mulib_hattrie_t *trie, const void *key, size_t length,
                         uint64_t value);
  /** @brief Lookup; NULL key is allowed only with length zero; output is
   * optional. */
  int mulib_hattrie_get (const mulib_hattrie_t *trie, const void *key,
                         size_t length, uint64_t *value);
  /** @brief Delete without allocation, pruning empty nodes. Return 1 if found.
   */
  int mulib_hattrie_remove (mulib_hattrie_t *trie, const void *key,
                            size_t length, uint64_t *old_value);
  /** @brief Enumerate keys starting with prefix, including an exact match.
   * Return number of callbacks, including a callback which stops iteration. */
  size_t mulib_hattrie_prefix (const mulib_hattrie_t *trie, const void *prefix,
                               size_t length, mulib_hattrie_visit_fn visit,
                               void *context);
  /** @brief Check parent links, hash placement, lookup reachability, and size.
   */
  int mulib_hattrie_validate (const mulib_hattrie_t *trie);
  /** @brief Iteratively release keys/nodes and reset; no recursive stack
   * growth. */
  void mulib_hattrie_free (mulib_hattrie_t *trie);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HATTRIE_H */

#if defined(MULIB_IMPL_HATTRIE) && !defined(MULIB_HATTRIE_IMPLEMENTED)
#define MULIB_HATTRIE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_hattrie_entry
  {
    struct mulib_hattrie_entry *next;
    uint64_t hash, value;
    size_t length;
    unsigned char key[];
  } mulib_hattrie_entry_t;
  struct mulib_hattrie_node
  {
    struct mulib_hattrie_node *parent;
    unsigned int edge;
    int branch;
    size_t count, prefix_length;
    unsigned char *prefix;
    mulib_hattrie_entry_t *terminal;
    union
    {
      struct mulib_hattrie_node *child[256];
      mulib_hattrie_entry_t *bucket[32];
    } u;
  };
  void
  mulib_hattrie_init (mulib_hattrie_t *t)
  {
    if (t)
      {
        t->root = NULL;
        t->size = 0;
      }
  }
  static mulib_hattrie_node_t *
  mulib_hattrie_node_ (int branch)
  {
    mulib_hattrie_node_t *n = (mulib_hattrie_node_t *)calloc (1, sizeof (*n));
    if (n)
      n->branch = branch;
    return n;
  }
  static void
  mulib_hattrie_bucket_add_ (mulib_hattrie_node_t *n, mulib_hattrie_entry_t *e)
  {
    size_t i = (size_t)(e->hash & 31);
    e->next = n->u.bucket[i];
    n->u.bucket[i] = e;
    ++n->count;
  }
  static int
  mulib_hattrie_equal_ (const mulib_hattrie_entry_t *e, const void *k,
                        size_t len, uint64_t hash)
  {
    return e->hash == hash && e->length == len
           && (!len || !memcmp (e->key, k, len));
  }
  static mulib_hattrie_entry_t *
  mulib_hattrie_find_ (const mulib_hattrie_t *t, const unsigned char *k,
                       size_t len, uint64_t h, mulib_hattrie_node_t **node)
  {
    mulib_hattrie_node_t *n = t->root;
    size_t depth = 0;
    while (n && n->branch)
      {
        if (n->prefix_length > len - depth
            || (n->prefix_length
                && memcmp (n->prefix, k + depth, n->prefix_length)))
          return NULL;
        depth += n->prefix_length;
        if (depth == len)
          {
            if (node)
              *node = n;
            return n->terminal;
          }
        n = n->u.child[k[depth++]];
      }
    if (node)
      *node = n;
    if (n)
      {
        mulib_hattrie_entry_t *e = n->u.bucket[(size_t)(h & 31)];
        while (e)
          {
            if (mulib_hattrie_equal_ (e, k, len, h))
              return e;
            e = e->next;
          }
      }
    return NULL;
  }
  int
  mulib_hattrie_get (const mulib_hattrie_t *t, const void *k, size_t len,
                     uint64_t *out)
  {
    mulib_hattrie_entry_t *e;
    if (!t || (!k && len))
      return 0;
    e = mulib_hattrie_find_ (t, (const unsigned char *)k, len,
                             mulib_hash_fnv1a (k, len), NULL);
    if (!e)
      return 0;
    if (out)
      *out = e->value;
    return 1;
  }
  static unsigned char *
  mulib_hattrie_copy_ (const unsigned char *p, size_t n)
  {
    unsigned char *s;
    if (!n)
      return NULL;
    s = (unsigned char *)malloc (n);
    if (s)
      memcpy (s, p, n);
    return s;
  }
  /* Build a whole burst before moving any entries; OOM leaves the bucket
   * intact. */
  static void
  mulib_hattrie_burst_ (mulib_hattrie_node_t **slot, size_t depth)
  {
    mulib_hattrie_node_t *old = *slot, *b;
    mulib_hattrie_entry_t *first = NULL;
    size_t common = 0, i;
    for (i = 0; i < 32 && !first; ++i)
      first = old->u.bucket[i];
    if (!first)
      return;
    common = first->length - depth;
    for (i = 0; i < 32; ++i)
      {
        mulib_hattrie_entry_t *e;
        for (e = old->u.bucket[i]; e; e = e->next)
          {
            size_t j = 0, limit = e->length - depth;
            if (limit > common)
              limit = common;
            while (j < limit && first->key[depth + j] == e->key[depth + j])
              ++j;
            common = j;
          }
      }
    b = mulib_hattrie_node_ (1);
    if (!b)
      return;
    b->prefix = mulib_hattrie_copy_ (first->key + depth, common);
    if (common && !b->prefix)
      {
        free (b);
        return;
      }
    b->prefix_length = common;
    b->parent = old->parent;
    b->edge = old->edge;
    for (i = 0; i < 32; ++i)
      {
        mulib_hattrie_entry_t *e;
        for (e = old->u.bucket[i]; e; e = e->next)
          {
            if (e->length > depth + common)
              {
                unsigned int edge = e->key[depth + common];
                if (!b->u.child[edge])
                  {
                    b->u.child[edge] = mulib_hattrie_node_ (0);
                    if (!b->u.child[edge])
                      {
                        for (size_t j = 0; j < 256; ++j)
                          free (b->u.child[j]);
                        free (b->prefix);
                        free (b);
                        return;
                      }
                    b->u.child[edge]->parent = b;
                    b->u.child[edge]->edge = edge;
                  }
              }
          }
      }
    for (i = 0; i < 32; ++i)
      {
        mulib_hattrie_entry_t *e = old->u.bucket[i];
        while (e)
          {
            mulib_hattrie_entry_t *next = e->next;
            if (e->length == depth + common)
              {
                b->terminal = e;
                e->next = NULL;
              }
            else
              mulib_hattrie_bucket_add_ (b->u.child[e->key[depth + common]],
                                         e);
            e = next;
          }
      }
    *slot = b;
    free (old);
  }
  int
  mulib_hattrie_set (mulib_hattrie_t *t, const void *key, size_t len,
                     uint64_t value)
  {
    const unsigned char *k = (const unsigned char *)key;
    mulib_hattrie_node_t **slot, *n, *parent = NULL;
    mulib_hattrie_entry_t *e;
    size_t depth = 0;
    unsigned int edge = 0;
    uint64_t hash;
    if (!t || (!k && len))
      return 0;
    hash = mulib_hash_fnv1a (k, len);
    e = mulib_hattrie_find_ (t, k, len, hash, NULL);
    if (e)
      {
        e->value = value;
        return 1;
      }
    if (t->size == SIZE_MAX || len > SIZE_MAX - sizeof (*e))
      return 0;
    e = (mulib_hattrie_entry_t *)malloc (sizeof (*e) + len);
    if (!e)
      return 0;
    e->hash = hash;
    e->value = value;
    e->length = len;
    e->next = NULL;
    if (len)
      memcpy (e->key, k, len);
    slot = &t->root;
    while ((n = *slot) != NULL && n->branch)
      {
        size_t common = 0;
        while (common < n->prefix_length && common < len - depth
               && n->prefix[common] == k[depth + common])
          ++common;
        if (common < n->prefix_length)
          {
            mulib_hattrie_node_t *b = mulib_hattrie_node_ (1), *leaf = NULL;
            unsigned char *suffix = NULL;
            unsigned int old_edge = n->prefix[common];
            if (!b)
              {
                free (e);
                return 0;
              }
            b->prefix = mulib_hattrie_copy_ (n->prefix, common);
            suffix = mulib_hattrie_copy_ (n->prefix + common + 1,
                                          n->prefix_length - common - 1);
            if (len > depth + common)
              leaf = mulib_hattrie_node_ (0);
            if ((common && !b->prefix)
                || (n->prefix_length > common + 1 && !suffix)
                || (len > depth + common && !leaf))
              {
                free (b->prefix);
                free (suffix);
                free (leaf);
                free (b);
                free (e);
                return 0;
              }
            b->prefix_length = common;
            b->parent = parent;
            b->edge = edge;
            b->u.child[old_edge] = n;
            if (leaf)
              {
                unsigned int new_edge = k[depth + common];
                b->u.child[new_edge] = leaf;
                leaf->parent = b;
                leaf->edge = new_edge;
                mulib_hattrie_bucket_add_ (leaf, e);
              }
            else
              b->terminal = e;
            free (n->prefix);
            n->prefix = suffix;
            n->prefix_length -= common + 1;
            n->parent = b;
            n->edge = old_edge;
            *slot = b;
            ++t->size;
            return 1;
          }
        depth += common;
        if (depth == len)
          {
            n->terminal = e;
            ++t->size;
            return 1;
          }
        edge = k[depth++];
        parent = n;
        slot = &n->u.child[edge];
      }
    if (!n)
      {
        n = mulib_hattrie_node_ (0);
        if (!n)
          {
            free (e);
            return 0;
          }
        n->parent = parent;
        n->edge = edge;
        *slot = n;
      }
    mulib_hattrie_bucket_add_ (n, e);
    ++t->size;
    if (n->count > 16)
      mulib_hattrie_burst_ (slot, depth);
    return 1;
  }
  static void
  mulib_hattrie_detach_ (mulib_hattrie_t *t, mulib_hattrie_node_t *n)
  {
    if (n->parent)
      n->parent->u.child[n->edge] = NULL;
    else
      t->root = NULL;
    free (n->prefix);
    free (n);
  }
  int
  mulib_hattrie_remove (mulib_hattrie_t *t, const void *key, size_t len,
                        uint64_t *out)
  {
    mulib_hattrie_node_t *n = NULL;
    mulib_hattrie_entry_t *e;
    uint64_t h;
    if (!t || (!key && len))
      return 0;
    h = mulib_hash_fnv1a (key, len);
    e = mulib_hattrie_find_ (t, (const unsigned char *)key, len, h, &n);
    if (!e)
      return 0;
    if (out)
      *out = e->value;
    if (n->branch)
      n->terminal = NULL;
    else
      {
        mulib_hattrie_entry_t **p = &n->u.bucket[(size_t)(h & 31)];
        while (*p != e)
          p = &(*p)->next;
        *p = e->next;
        --n->count;
      }
    free (e);
    --t->size;
    while (n)
      {
        mulib_hattrie_node_t *parent = n->parent;
        if (n->branch)
          {
            size_t i;
            if (n->terminal)
              break;
            for (i = 0; i < 256 && !n->u.child[i]; ++i)
              {
              }
            if (i != 256)
              break;
          }
        else if (n->count)
          break;
        mulib_hattrie_detach_ (t, n);
        n = parent;
      }
    return 1;
  }
  /* Preorder successor within a subtree, without a recursion stack. */
  static mulib_hattrie_node_t *
  mulib_hattrie_next_ (mulib_hattrie_node_t *n,
                       const mulib_hattrie_node_t *root)
  {
    size_t i;
    if (n->branch)
      for (i = 0; i < 256; ++i)
        if (n->u.child[i])
          return n->u.child[i];
    while (n != root)
      {
        mulib_hattrie_node_t *p = n->parent;
        for (i = (size_t)n->edge + 1; i < 256; ++i)
          if (p->u.child[i])
            return p->u.child[i];
        n = p;
      }
    return NULL;
  }
  size_t
  mulib_hattrie_prefix (const mulib_hattrie_t *t, const void *prefix,
                        size_t len, mulib_hattrie_visit_fn fn, void *ctx)
  {
    const unsigned char *p = (const unsigned char *)prefix;
    mulib_hattrie_node_t *n, *root;
    size_t depth = 0, count = 0;
    if (!t || (!p && len) || !fn)
      return 0;
    n = t->root;
    while (n && n->branch && depth < len)
      {
        size_t take = len - depth;
        if (take > n->prefix_length)
          take = n->prefix_length;
        if (take && memcmp (n->prefix, p + depth, take))
          return 0;
        depth += take;
        if (depth == len)
          break;
        n = n->u.child[p[depth++]];
      }
    root = n;
    while (n)
      {
        if (n->branch)
          {
            mulib_hattrie_entry_t *e = n->terminal;
            if (e && e->length >= len && (!len || !memcmp (e->key, p, len)))
              {
                ++count;
                if (!fn (e->key, e->length, e->value, ctx))
                  return count;
              }
          }
        else
          for (size_t i = 0; i < 32; ++i)
            {
              mulib_hattrie_entry_t *e;
              for (e = n->u.bucket[i]; e; e = e->next)
                {
                  if (e->length >= len && (!len || !memcmp (e->key, p, len)))
                    {
                      ++count;
                      if (!fn (e->key, e->length, e->value, ctx))
                        return count;
                    }
                }
            }
        n = mulib_hattrie_next_ (n, root);
      }
    return count;
  }
  static int
  mulib_hattrie_check_entry_ (const mulib_hattrie_t *t,
                              const mulib_hattrie_entry_t *e, size_t *count)
  {
    if (*count == SIZE_MAX || e->hash != mulib_hash_fnv1a (e->key, e->length)
        || mulib_hattrie_find_ (t, e->key, e->length, e->hash, NULL) != e)
      return 0;
    ++*count;
    return 1;
  }
  int
  mulib_hattrie_validate (const mulib_hattrie_t *t)
  {
    mulib_hattrie_node_t *n;
    size_t total = 0;
    if (!t)
      return 0;
    if (!t->root)
      return !t->size;
    if (t->root->parent)
      return 0;
    for (n = t->root; n; n = mulib_hattrie_next_ (n, t->root))
      {
        if (n->branch)
          {
            if (n->prefix_length && !n->prefix)
              return 0;
            if (n->terminal
                && !mulib_hattrie_check_entry_ (t, n->terminal, &total))
              return 0;
            for (size_t i = 0; i < 256; ++i)
              {
                if (n->u.child[i]
                    && (n->u.child[i]->parent != n
                        || n->u.child[i]->edge != i))
                  return 0;
              }
          }
        else
          {
            size_t count = 0;
            for (size_t i = 0; i < 32; ++i)
              {
                mulib_hattrie_entry_t *e;
                for (e = n->u.bucket[i]; e; e = e->next)
                  {
                    if ((e->hash & 31) != i
                        || !mulib_hattrie_check_entry_ (t, e, &total))
                      return 0;
                    ++count;
                  }
              }
            if (!count || count != n->count)
              return 0;
          }
      }
    return total == t->size;
  }
  void
  mulib_hattrie_free (mulib_hattrie_t *t)
  {
    mulib_hattrie_node_t *n;
    if (!t)
      return;
    n = t->root;
    while (n)
      {
        mulib_hattrie_node_t *child = NULL, *parent;
        if (n->branch)
          {
            for (size_t i = 0; i < 256; ++i)
              if (n->u.child[i])
                {
                  child = n->u.child[i];
                  n->u.child[i] = NULL;
                  break;
                }
            if (child)
              {
                n = child;
                continue;
              }
            free (n->terminal);
          }
        else
          for (size_t i = 0; i < 32; ++i)
            {
              mulib_hattrie_entry_t *e = n->u.bucket[i];
              while (e)
                {
                  mulib_hattrie_entry_t *next = e->next;
                  free (e);
                  e = next;
                }
            }
        parent = n->parent;
        free (n->prefix);
        free (n);
        n = parent;
      }
    mulib_hattrie_init (t);
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
