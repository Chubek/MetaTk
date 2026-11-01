/**
 * @file mulib-sppf.h
 * @brief Shared packed parse forests, including nullable and cyclic forests.
 *
 * Symbol nodes share (kind, symbol, begin, end). A packed alternative shares
 * (parent, production, ordered children). Define MULIB_IMPL_SPPF and provide
 * MULIB_IMPL_VEC in one translation unit. See manual/mulib-sppf.md.
 */
#ifndef MULIB_SPPF_H
#define MULIB_SPPF_H
#include "mulib-vec.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

/** Missing node or alternative ID. */
#define MULIB_SPPF_NONE SIZE_MAX
  /** Results; INFINITE is used only by count. */
  typedef enum mulib_sppf_status
  {
    MULIB_SPPF_OK = 0,
    MULIB_SPPF_INFINITE = 1,
    MULIB_SPPF_INVALID = -1,
    MULIB_SPPF_NOMEM = -2,
    MULIB_SPPF_LIMIT = -3
  } mulib_sppf_status_t;
  /** Terminal nodes consume one token; nonterminals may cover an empty span.
   */
  typedef enum mulib_sppf_kind
  {
    MULIB_SPPF_TERMINAL = 0,
    MULIB_SPPF_NONTERMINAL = 1
  } mulib_sppf_kind_t;
  /** Opaque owning forest. Symbols and production IDs are application labels.
   */
  typedef struct mulib_sppf mulib_sppf_t;
  /** A symbol and half-open input span; first_alternative may be NONE. */
  typedef struct mulib_sppf_node
  {
    mulib_sppf_kind_t kind;
    size_t symbol, begin, end, first_alternative;
  } mulib_sppf_node_t;
  /** One production choice; use child() for its ordered child node IDs. */
  typedef struct mulib_sppf_alternative
  {
    size_t parent, production, child_count, next;
  } mulib_sppf_alternative_t;

  /** Zero limits select 65536 nodes, 262144 alternatives, 1048576 child IDs.
   */
  mulib_sppf_t *mulib_sppf_create (size_t max_nodes, size_t max_alternatives,
                                   size_t max_children);
  /** Free the forest; NULL is allowed. */
  void mulib_sppf_destroy (mulib_sppf_t *forest);
  /** Remove all entries, retaining capacity; invalidates every ID. */
  void mulib_sppf_clear (mulib_sppf_t *forest);
  /** Find/create a symbol node. out_id is required; created may be NULL. */
  int mulib_sppf_intern (mulib_sppf_t *forest, mulib_sppf_kind_t kind,
                         size_t symbol, size_t begin, size_t end,
                         size_t *out_id, int *created);
  /**
   * Find/create an alternative of a nonterminal. Children must exist and their
   * spans must concatenate to the parent span. Empty children require an empty
   * parent span. Self-references/cycles are valid. Failure changes no entries.
   */
  int mulib_sppf_pack (mulib_sppf_t *forest, size_t parent, size_t production,
                       const size_t *children, size_t child_count,
                       size_t *out_id, int *created);
  /** Copy a symbol node; returns INVALID for missing IDs/NULL outputs. */
  int mulib_sppf_node (const mulib_sppf_t *forest, size_t id,
                       mulib_sppf_node_t *out);
  /** Copy an alternative; follow next to enumerate the parent's choices. */
  int mulib_sppf_alternative (const mulib_sppf_t *forest, size_t id,
                              mulib_sppf_alternative_t *out);
  /** Copy one ordered child node ID. */
  int mulib_sppf_child (const mulib_sppf_t *forest, size_t alternative,
                        size_t index, size_t *out_id);
  /** Number of interned symbol nodes (zero for NULL). */
  size_t mulib_sppf_node_count (const mulib_sppf_t *forest);
  /** Number of packed alternatives (zero for NULL). */
  size_t mulib_sppf_alternative_count (const mulib_sppf_t *forest);
  /**
   * Count finite derivation trees without recursion. cap must be positive.
   * OK gives an exact count, INFINITE gives cap, LIMIT gives cap if a finite
   * count exceeds cap, or zero if the work budget is exhausted. Zero max_steps
   * selects 10000000. Unproductive cycles count as zero, not infinity.
   */
  int mulib_sppf_count (const mulib_sppf_t *forest, size_t root, size_t cap,
                        size_t max_steps, size_t *out_count);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPPF_H */

#if defined(MULIB_IMPL_SPPF) && !defined(MULIB_SPPF_IMPLEMENTED)
#define MULIB_SPPF_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_sppf_packed_
  {
    mulib_sppf_alternative_t value;
    size_t offset;
  } mulib_sppf_packed_;
  struct mulib_sppf
  {
    mulib_vec_t nodes, alternatives, children;
    size_t max_nodes, max_alternatives, max_children;
  };
  typedef struct mulib_sppf_count_cell_
  {
    size_t count;
    unsigned char productive, reachable, resolved, capped;
  } mulib_sppf_count_cell_;

  static const mulib_sppf_node_t *
  mulib_sppf_node_ (const mulib_sppf_t *f, size_t i)
  {
    return (const mulib_sppf_node_t *)(f->nodes.data
                                       + i * sizeof (mulib_sppf_node_t));
  }
  static const mulib_sppf_packed_ *
  mulib_sppf_packed_at_ (const mulib_sppf_t *f, size_t i)
  {
    return (const mulib_sppf_packed_ *)(f->alternatives.data
                                        + i * sizeof (mulib_sppf_packed_));
  }
  static size_t
  mulib_sppf_child_ (const mulib_sppf_t *f, size_t i)
  {
    return ((const size_t *)f->children.data)[i];
  }
  static int
  mulib_sppf_tick_ (size_t *work, size_t limit)
  {
    if (*work >= limit)
      return 0;
    ++*work;
    return 1;
  }

  mulib_sppf_t *
  mulib_sppf_create (size_t nodes, size_t alternatives, size_t children)
  {
    mulib_sppf_t *f = (mulib_sppf_t *)malloc (sizeof (*f));
    if (!f)
      return NULL;
    mulib_vec_init (&f->nodes, sizeof (mulib_sppf_node_t));
    mulib_vec_init (&f->alternatives, sizeof (mulib_sppf_packed_));
    mulib_vec_init (&f->children, sizeof (size_t));
    f->max_nodes = nodes ? nodes : 65536;
    f->max_alternatives = alternatives ? alternatives : 262144;
    f->max_children = children ? children : 1048576;
    return f;
  }
  void
  mulib_sppf_destroy (mulib_sppf_t *f)
  {
    if (!f)
      return;
    mulib_vec_free (&f->nodes);
    mulib_vec_free (&f->alternatives);
    mulib_vec_free (&f->children);
    free (f);
  }
  void
  mulib_sppf_clear (mulib_sppf_t *f)
  {
    if (!f)
      return;
    mulib_vec_clear (&f->nodes);
    mulib_vec_clear (&f->alternatives);
    mulib_vec_clear (&f->children);
  }

  int
  mulib_sppf_intern (mulib_sppf_t *f, mulib_sppf_kind_t kind, size_t symbol,
                     size_t begin, size_t end, size_t *out_id, int *created)
  {
    size_t i;
    mulib_sppf_node_t node;
    if (created)
      *created = 0;
    if (!f || !out_id || begin > end
        || (kind != MULIB_SPPF_TERMINAL && kind != MULIB_SPPF_NONTERMINAL)
        || (kind == MULIB_SPPF_TERMINAL && end - begin != 1))
      return MULIB_SPPF_INVALID;
    for (i = 0; i < f->nodes.length; ++i)
      {
        const mulib_sppf_node_t *n = mulib_sppf_node_ (f, i);
        if (n->kind == kind && n->symbol == symbol && n->begin == begin
            && n->end == end)
          {
            *out_id = i;
            return MULIB_SPPF_OK;
          }
      }
    if (f->nodes.length >= f->max_nodes
        || f->nodes.length >= SIZE_MAX / sizeof (node))
      return MULIB_SPPF_LIMIT;
    node.kind = kind;
    node.symbol = symbol;
    node.begin = begin;
    node.end = end;
    node.first_alternative = MULIB_SPPF_NONE;
    if (!mulib_vec_push (&f->nodes, &node))
      return MULIB_SPPF_NOMEM;
    *out_id = f->nodes.length - 1;
    if (created)
      *created = 1;
    return MULIB_SPPF_OK;
  }

  int
  mulib_sppf_pack (mulib_sppf_t *f, size_t parent, size_t production,
                   const size_t *children, size_t count, size_t *out_id,
                   int *created)
  {
    mulib_sppf_node_t *p;
    mulib_sppf_packed_ packed;
    size_t i, pos, total;
    if (created)
      *created = 0;
    if (!f || !out_id || parent >= f->nodes.length || (count && !children))
      return MULIB_SPPF_INVALID;
    if (count > SIZE_MAX / sizeof (size_t))
      return MULIB_SPPF_LIMIT;
    p = (mulib_sppf_node_t *)mulib_vec_at (&f->nodes, parent);
    if (p->kind != MULIB_SPPF_NONTERMINAL)
      return MULIB_SPPF_INVALID;
    if (count > f->max_children)
      return MULIB_SPPF_LIMIT;
    pos = p->begin;
    for (i = 0; i < count; ++i)
      {
        const mulib_sppf_node_t *child;
        if (children[i] >= f->nodes.length)
          return MULIB_SPPF_INVALID;
        child = mulib_sppf_node_ (f, children[i]);
        if (child->begin != pos)
          return MULIB_SPPF_INVALID;
        pos = child->end;
      }
    if (pos != p->end)
      return MULIB_SPPF_INVALID;
    for (i = p->first_alternative; i != MULIB_SPPF_NONE;)
      {
        const mulib_sppf_packed_ *a = mulib_sppf_packed_at_ (f, i);
        if (a->value.production == production && a->value.child_count == count
            && (!count
                || memcmp (f->children.data + a->offset * sizeof (size_t),
                           children, count * sizeof (size_t))
                       == 0))
          {
            *out_id = i;
            return MULIB_SPPF_OK;
          }
        i = a->value.next;
      }
    if (f->alternatives.length >= f->max_alternatives
        || f->alternatives.length >= SIZE_MAX / sizeof (packed)
        || count > SIZE_MAX - f->children.length)
      return MULIB_SPPF_LIMIT;
    total = f->children.length + count;
    if (total > f->max_children || total > SIZE_MAX / sizeof (size_t))
      return MULIB_SPPF_LIMIT;
    if (!mulib_vec_reserve (&f->children, total)
        || !mulib_vec_reserve (&f->alternatives, f->alternatives.length + 1))
      return MULIB_SPPF_NOMEM;
    packed.value.parent = parent;
    packed.value.production = production;
    packed.value.child_count = count;
    packed.value.next = p->first_alternative;
    packed.offset = f->children.length;
    if (count)
      memcpy (f->children.data + packed.offset * sizeof (size_t), children,
              count * sizeof (size_t));
    f->children.length = total;
    /* Capacity is reserved, so this push cannot allocate or fail. */
    (void)mulib_vec_push (&f->alternatives, &packed);
    p->first_alternative = f->alternatives.length - 1;
    *out_id = p->first_alternative;
    if (created)
      *created = 1;
    return MULIB_SPPF_OK;
  }

  int
  mulib_sppf_node (const mulib_sppf_t *f, size_t id, mulib_sppf_node_t *out)
  {
    if (!f || !out || id >= f->nodes.length)
      return MULIB_SPPF_INVALID;
    *out = *mulib_sppf_node_ (f, id);
    return MULIB_SPPF_OK;
  }
  int
  mulib_sppf_alternative (const mulib_sppf_t *f, size_t id,
                          mulib_sppf_alternative_t *out)
  {
    if (!f || !out || id >= f->alternatives.length)
      return MULIB_SPPF_INVALID;
    *out = mulib_sppf_packed_at_ (f, id)->value;
    return MULIB_SPPF_OK;
  }
  int
  mulib_sppf_child (const mulib_sppf_t *f, size_t alternative, size_t index,
                    size_t *out_id)
  {
    const mulib_sppf_packed_ *a;
    if (!f || !out_id || alternative >= f->alternatives.length)
      return MULIB_SPPF_INVALID;
    a = mulib_sppf_packed_at_ (f, alternative);
    if (index >= a->value.child_count)
      return MULIB_SPPF_INVALID;
    *out_id = mulib_sppf_child_ (f, a->offset + index);
    return MULIB_SPPF_OK;
  }
  size_t
  mulib_sppf_node_count (const mulib_sppf_t *f)
  {
    return f ? f->nodes.length : 0;
  }
  size_t
  mulib_sppf_alternative_count (const mulib_sppf_t *f)
  {
    return f ? f->alternatives.length : 0;
  }

  /* 1: productive alternative, 0: unproductive, -1: work exhausted. */
  static int
  mulib_sppf_productive_ (const mulib_sppf_t *f, const mulib_sppf_packed_ *a,
                          const mulib_sppf_count_cell_ *cells, size_t *work,
                          size_t limit)
  {
    size_t j;
    if (!mulib_sppf_tick_ (work, limit))
      return -1;
    for (j = 0; j < a->value.child_count; ++j)
      {
        if (!mulib_sppf_tick_ (work, limit))
          return -1;
        if (!cells[mulib_sppf_child_ (f, a->offset + j)].productive)
          return 0;
      }
    return 1;
  }

  int
  mulib_sppf_count (const mulib_sppf_t *f, size_t root, size_t cap,
                    size_t max_steps, size_t *out_count)
  {
    mulib_sppf_count_cell_ *cells;
    mulib_vec_t queue;
    size_t i, j, q, work = 0, remaining = 0;
    int changed, status = MULIB_SPPF_OK;
    if (out_count)
      *out_count = 0;
    if (!f || !out_count || !cap || root >= f->nodes.length)
      return MULIB_SPPF_INVALID;
    if (f->nodes.length > SIZE_MAX / sizeof (*cells))
      return MULIB_SPPF_LIMIT;
    if (!max_steps)
      max_steps = 10000000;
    cells
        = (mulib_sppf_count_cell_ *)calloc (f->nodes.length, sizeof (*cells));
    if (!cells)
      return MULIB_SPPF_NOMEM;
    mulib_vec_init (&queue, sizeof (size_t));
    for (i = 0; i < f->nodes.length; ++i)
      {
        if (!mulib_sppf_tick_ (&work, max_steps))
          goto work_limit;
        if (mulib_sppf_node_ (f, i)->kind == MULIB_SPPF_TERMINAL)
          cells[i].productive = 1;
      }
    do
      {
        changed = 0;
        for (i = 0; i < f->alternatives.length; ++i)
          {
            const mulib_sppf_packed_ *a = mulib_sppf_packed_at_ (f, i);
            int productive;
            if (!mulib_sppf_tick_ (&work, max_steps))
              goto work_limit;
            if (cells[a->value.parent].productive)
              continue;
            productive
                = mulib_sppf_productive_ (f, a, cells, &work, max_steps);
            if (productive < 0)
              goto work_limit;
            if (productive)
              {
                cells[a->value.parent].productive = 1;
                changed = 1;
              }
          }
      }
    while (changed);
    if (!cells[root].productive)
      goto done;
    cells[root].reachable = 1;
    if (!mulib_vec_push (&queue, &root))
      {
        status = MULIB_SPPF_NOMEM;
        goto done;
      }
    for (q = 0; q < queue.length; ++q)
      {
        size_t node = ((const size_t *)queue.data)[q];
        const mulib_sppf_node_t *n = mulib_sppf_node_ (f, node);
        if (!mulib_sppf_tick_ (&work, max_steps))
          goto work_limit;
        if (n->kind == MULIB_SPPF_TERMINAL)
          {
            cells[node].resolved = 1;
            cells[node].count = 1;
            continue;
          }
        ++remaining;
        for (i = n->first_alternative; i != MULIB_SPPF_NONE;)
          {
            const mulib_sppf_packed_ *a = mulib_sppf_packed_at_ (f, i);
            int productive
                = mulib_sppf_productive_ (f, a, cells, &work, max_steps);
            if (productive < 0)
              goto work_limit;
            if (productive)
              for (j = 0; j < a->value.child_count; ++j)
                {
                  size_t child = mulib_sppf_child_ (f, a->offset + j);
                  if (!mulib_sppf_tick_ (&work, max_steps))
                    goto work_limit;
                  if (!cells[child].reachable)
                    {
                      if (!mulib_vec_push (&queue, &child))
                        {
                          status = MULIB_SPPF_NOMEM;
                          goto done;
                        }
                      cells[child].reachable = 1;
                    }
                }
            i = a->value.next;
          }
      }
    do
      {
        changed = 0;
        for (q = 0; q < queue.length; ++q)
          {
            size_t node = ((const size_t *)queue.data)[q], total = 0;
            int ready = 1, total_capped = 0;
            const mulib_sppf_node_t *n = mulib_sppf_node_ (f, node);
            if (!mulib_sppf_tick_ (&work, max_steps))
              goto work_limit;
            if (cells[node].resolved)
              continue;
            for (i = n->first_alternative; i != MULIB_SPPF_NONE;)
              {
                const mulib_sppf_packed_ *a = mulib_sppf_packed_at_ (f, i);
                size_t product = 1;
                int product_capped = 0;
                int productive
                    = mulib_sppf_productive_ (f, a, cells, &work, max_steps);
                if (productive < 0)
                  goto work_limit;
                if (productive)
                  {
                    for (j = 0; j < a->value.child_count; ++j)
                      {
                        const mulib_sppf_count_cell_ *c
                            = cells + mulib_sppf_child_ (f, a->offset + j);
                        if (!mulib_sppf_tick_ (&work, max_steps))
                          goto work_limit;
                        if (!c->resolved)
                          {
                            ready = 0;
                            break;
                          }
                        if (c->capped || product > cap / c->count)
                          {
                            product = cap;
                            product_capped = 1;
                          }
                        else
                          product *= c->count;
                      }
                    if (!ready)
                      break;
                    if (product_capped || product > cap - total)
                      {
                        total = cap;
                        total_capped = 1;
                      }
                    else
                      total += product;
                  }
                i = a->value.next;
              }
            if (ready)
              {
                cells[node].resolved = 1;
                cells[node].count = total;
                cells[node].capped = (unsigned char)total_capped;
                --remaining;
                changed = 1;
              }
          }
      }
    while (changed && remaining);
    if (remaining)
      {
        *out_count = cap;
        status = MULIB_SPPF_INFINITE;
      }
    else
      {
        *out_count = cells[root].count;
        if (cells[root].capped)
          status = MULIB_SPPF_LIMIT;
      }
    goto done;
  work_limit:
    status = MULIB_SPPF_LIMIT;
  done:
    mulib_vec_free (&queue);
    free (cells);
    return status;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
