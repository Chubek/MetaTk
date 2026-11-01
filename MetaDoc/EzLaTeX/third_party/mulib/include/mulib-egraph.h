/** @file mulib-egraph.h
 * @brief Bounded binary e-graphs: hash-consing, union, congruence rebuild and
 * extraction. Define MULIB_IMPL_EGRAPH. No dependencies. Not a concurrent
 * container.
 */
#ifndef MULIB_EGRAPH_H
#define MULIB_EGRAPH_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
#define MULIB_EGRAPH_NONE SIZE_MAX
  typedef struct mulib_egraph mulib_egraph_t;
  /** Keys include opcode, literal, arity and canonical child classes. Unused
   * children ignored. */
  typedef struct mulib_egraph_node
  {
    unsigned opcode, arity;
    int64_t literal;
    size_t child[2];
  } mulib_egraph_node_t;
  typedef enum mulib_egraph_status
  {
    MULIB_EGRAPH_OK,
    MULIB_EGRAPH_INVALID,
    MULIB_EGRAPH_RESOURCE_LIMIT,
    MULIB_EGRAPH_NO_MEMORY,
    MULIB_EGRAPH_DIRTY,
    MULIB_EGRAPH_CYCLE,
    MULIB_EGRAPH_OVERFLOW
  } mulib_egraph_status_t;
  /** Allocate fixed storage for at most node_limit enodes/classes. Caller
   * destroys it. *output must be NULL. Zero capacity is valid. No allocations
   * during add/union/rebuild.
   */
  mulib_egraph_status_t mulib_egraph_create (size_t node_limit,
                                             mulib_egraph_t **output);
  /** Destroy owned graph; NULL is harmless. */
  void mulib_egraph_destroy (mulib_egraph_t *graph);
  /** Hash-cons a node into a clean graph; return its class. Child IDs must
   * exist. Existing keys do not consume capacity. Failure does not add a node.
   */
  mulib_egraph_status_t mulib_egraph_add (mulib_egraph_t *graph,
                                          mulib_egraph_node_t node,
                                          size_t *class_id);
  /** Union two classes. Successful nontrivial unions mark the hash table
   * dirty. changed may be NULL. Congruence is restored only by explicit
   * rebuild.
   */
  mulib_egraph_status_t mulib_egraph_union (mulib_egraph_t *graph, size_t a,
                                            size_t b, int *changed);
  /** Canonical representative, or NONE for invalid ID. Valid even while dirty.
   */
  size_t mulib_egraph_find (mulib_egraph_t *graph, size_t class_id);
  /** Repeated hash-consing/union to congruence closure. One unit of work per
   * enode per pass. On limit exhaustion unions remain valid but graph stays
   * dirty; a later rebuild can resume (passes restart). work_used may be NULL.
   */
  mulib_egraph_status_t mulib_egraph_rebuild (mulib_egraph_t *graph,
                                              size_t work_limit,
                                              size_t *work_used);
  /** Number of enodes (including equivalent alternatives), or zero for NULL.
   */
  size_t mulib_egraph_count (const mulib_egraph_t *graph);
  /** Monotonic change counter: one per new node or nontrivial union. */
  size_t mulib_egraph_revision (const mulib_egraph_t *graph);
  /** Read a canonicalized enode by stable enode index and its containing
   * class. Either output may be NULL. Enode indices are not necessarily class
   * representatives.
   */
  mulib_egraph_status_t mulib_egraph_node (mulib_egraph_t *graph, size_t index,
                                           mulib_egraph_node_t *node,
                                           size_t *class_id);
  typedef uint64_t (*mulib_egraph_cost_fn) (const mulib_egraph_node_t *node,
                                            void *user);
  typedef struct mulib_egraph_extract_limits
  {
    size_t work, nodes;
  } mulib_egraph_extract_limits_t;
  /** Owned expanded tree, postorder; children index earlier records,
   * root=count-1. */
  typedef struct mulib_egraph_tree
  {
    mulib_egraph_node_t *nodes;
    size_t count, root;
    uint64_t cost;
  } mulib_egraph_tree_t;
  /** Extract minimum additive nonnegative cost (NULL callback means unit
   * cost). Cyclic classes with finite alternatives are supported; classes with
   * no finite tree return CYCLE. Overflowing alternatives are skipped. Ties
   * retain first choice. Requires a clean graph, empty output, and a pure
   * callback that does not mutate graph. work counts DP enode visits and tree
   * frame visits; nodes bounds expanded output.
   */
  mulib_egraph_status_t
  mulib_egraph_extract (mulib_egraph_t *graph, size_t root,
                        mulib_egraph_cost_fn cost, void *user,
                        mulib_egraph_extract_limits_t limits,
                        mulib_egraph_tree_t *output);
  /** Release extracted tree and zero it. */
  void mulib_egraph_tree_free (mulib_egraph_tree_t *tree);
  /** Static status description. */
  const char *mulib_egraph_status_string (mulib_egraph_status_t status);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EGRAPH) && !defined(MULIB_EGRAPH_IMPLEMENTED)
#define MULIB_EGRAPH_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  struct mulib_egraph
  {
    mulib_egraph_node_t *nodes;
    size_t *parents, *slots;
    size_t count, capacity, slot_count, revision;
    int dirty;
  };
  size_t
  mulib_egraph_count (const mulib_egraph_t *g)
  {
    return g ? g->count : 0;
  }
  size_t
  mulib_egraph_revision (const mulib_egraph_t *g)
  {
    return g ? g->revision : 0;
  }
  size_t
  mulib_egraph_find (mulib_egraph_t *g, size_t c)
  {
    size_t root, next;
    if (!g || c >= g->count)
      return MULIB_EGRAPH_NONE;
    root = c;
    while (g->parents[root] != root)
      root = g->parents[root];
    while (c != root)
      {
        next = g->parents[c];
        g->parents[c] = root;
        c = next;
      }
    return root;
  }
  void
  mulib_egraph_destroy (mulib_egraph_t *g)
  {
    if (g)
      {
        free (g->nodes);
        free (g->parents);
        free (g->slots);
        free (g);
      }
  }
  mulib_egraph_status_t
  mulib_egraph_create (size_t limit, mulib_egraph_t **out)
  {
    mulib_egraph_t *g;
    size_t slots = 2, k;
    if (!out || *out)
      return MULIB_EGRAPH_INVALID;
    if (limit > SIZE_MAX / 2 || limit > SIZE_MAX / sizeof (mulib_egraph_node_t)
        || limit > SIZE_MAX / sizeof (size_t))
      return MULIB_EGRAPH_RESOURCE_LIMIT;
    while (slots < limit * 2)
      {
        if (slots > SIZE_MAX / 2)
          return MULIB_EGRAPH_RESOURCE_LIMIT;
        slots *= 2;
      }
    if (slots > SIZE_MAX / sizeof (size_t))
      return MULIB_EGRAPH_RESOURCE_LIMIT;
    g = (mulib_egraph_t *)calloc (1, sizeof (*g));
    if (!g)
      return MULIB_EGRAPH_NO_MEMORY;
    g->capacity = limit;
    g->slot_count = slots;
    if (limit)
      {
        g->nodes = (mulib_egraph_node_t *)malloc (limit * sizeof (*g->nodes));
        g->parents = (size_t *)malloc (limit * sizeof (*g->parents));
      }
    g->slots = (size_t *)malloc (slots * sizeof (*g->slots));
    if ((limit && (!g->nodes || !g->parents)) || !g->slots)
      {
        mulib_egraph_destroy (g);
        return MULIB_EGRAPH_NO_MEMORY;
      }
    for (k = 0; k < slots; ++k)
      g->slots[k] = MULIB_EGRAPH_NONE;
    *out = g;
    return MULIB_EGRAPH_OK;
  }
  static size_t
  mulib_egraph__hash (const mulib_egraph_node_t *n)
  {
    uint64_t h = UINT64_C (1469598103934665603);
    unsigned j;
    h = (h ^ n->opcode) * UINT64_C (1099511628211);
    h = (h ^ n->arity) * UINT64_C (1099511628211);
    h = (h ^ (uint64_t)n->literal) * UINT64_C (1099511628211);
    for (j = 0; j < n->arity; ++j)
      h = (h ^ (uint64_t)n->child[j]) * UINT64_C (1099511628211);
    return (size_t)h;
  }
  static int
  mulib_egraph__equal (const mulib_egraph_node_t *a,
                       const mulib_egraph_node_t *b)
  {
    unsigned j;
    if (a->opcode != b->opcode || a->arity != b->arity
        || a->literal != b->literal)
      return 0;
    for (j = 0; j < a->arity; ++j)
      if (a->child[j] != b->child[j])
        return 0;
    return 1;
  }
  mulib_egraph_status_t
  mulib_egraph_node (mulib_egraph_t *g, size_t i, mulib_egraph_node_t *n,
                     size_t *c)
  {
    unsigned j;
    if (!g || i >= g->count)
      return MULIB_EGRAPH_INVALID;
    if (n)
      {
        *n = g->nodes[i];
        for (j = 0; j < n->arity; ++j)
          n->child[j] = mulib_egraph_find (g, n->child[j]);
      }
    if (c)
      *c = mulib_egraph_find (g, i);
    return MULIB_EGRAPH_OK;
  }
  mulib_egraph_status_t
  mulib_egraph_add (mulib_egraph_t *g, mulib_egraph_node_t n, size_t *c)
  {
    size_t slot, i;
    unsigned j;
    if (!g || !c || n.arity > 2)
      return MULIB_EGRAPH_INVALID;
    if (g->dirty)
      return MULIB_EGRAPH_DIRTY;
    for (j = 0; j < 2; ++j)
      {
        if (j < n.arity)
          {
            n.child[j] = mulib_egraph_find (g, n.child[j]);
            if (n.child[j] == MULIB_EGRAPH_NONE)
              return MULIB_EGRAPH_INVALID;
          }
        else
          n.child[j] = MULIB_EGRAPH_NONE;
      }
    slot = mulib_egraph__hash (&n) & (g->slot_count - 1);
    while ((i = g->slots[slot]) != MULIB_EGRAPH_NONE)
      {
        if (mulib_egraph__equal (&g->nodes[i], &n))
          {
            *c = mulib_egraph_find (g, i);
            return MULIB_EGRAPH_OK;
          }
        slot = (slot + 1) & (g->slot_count - 1);
      }
    if (g->count == g->capacity)
      return MULIB_EGRAPH_RESOURCE_LIMIT;
    i = g->count++;
    g->nodes[i] = n;
    g->parents[i] = i;
    g->slots[slot] = i;
    ++g->revision;
    *c = i;
    return MULIB_EGRAPH_OK;
  }
  mulib_egraph_status_t
  mulib_egraph_union (mulib_egraph_t *g, size_t a, size_t b, int *changed)
  {
    a = mulib_egraph_find (g, a);
    b = mulib_egraph_find (g, b);
    if (changed)
      *changed = 0;
    if (a == MULIB_EGRAPH_NONE || b == MULIB_EGRAPH_NONE)
      return MULIB_EGRAPH_INVALID;
    if (a != b)
      {
        if (a < b)
          g->parents[b] = a;
        else
          g->parents[a] = b;
        g->dirty = 1;
        ++g->revision;
        if (changed)
          *changed = 1;
      }
    return MULIB_EGRAPH_OK;
  }
  mulib_egraph_status_t
  mulib_egraph_rebuild (mulib_egraph_t *g, size_t limit, size_t *used)
  {
    size_t work = 0, i, slot, old;
    int changed;
    mulib_egraph_status_t s = MULIB_EGRAPH_OK;
    if (!g)
      {
        s = MULIB_EGRAPH_INVALID;
        goto finish;
      }
    if (!g->dirty)
      goto finish;
    do
      {
        changed = 0;
        for (i = 0; i < g->slot_count; ++i)
          g->slots[i] = MULIB_EGRAPH_NONE;
        for (i = 0; i < g->count; ++i)
          {
            unsigned j;
            if (work == limit)
              {
                s = MULIB_EGRAPH_RESOURCE_LIMIT;
                goto finish;
              }
            ++work;
            for (j = 0; j < g->nodes[i].arity; ++j)
              g->nodes[i].child[j]
                  = mulib_egraph_find (g, g->nodes[i].child[j]);
            slot = mulib_egraph__hash (&g->nodes[i]) & (g->slot_count - 1);
            while ((old = g->slots[slot]) != MULIB_EGRAPH_NONE
                   && !mulib_egraph__equal (&g->nodes[old], &g->nodes[i]))
              slot = (slot + 1) & (g->slot_count - 1);
            if (old == MULIB_EGRAPH_NONE)
              g->slots[slot] = i;
            else
              {
                int merged;
                (void)mulib_egraph_union (g, old, i, &merged);
                if (merged)
                  changed = 1;
              }
          }
      }
    while (changed);
    g->dirty = 0;
  finish:
    if (used)
      *used = work;
    return s;
  }
  void
  mulib_egraph_tree_free (mulib_egraph_tree_t *t)
  {
    if (t)
      {
        free (t->nodes);
        memset (t, 0, sizeof (*t));
      }
  }
  typedef struct mulib_egraph__frame
  {
    size_t cls, child[2];
    unsigned next;
  } mulib_egraph__frame;
  mulib_egraph_status_t
  mulib_egraph_extract (mulib_egraph_t *g, size_t root,
                        mulib_egraph_cost_fn fn, void *user,
                        mulib_egraph_extract_limits_t limits,
                        mulib_egraph_tree_t *out)
  {
    uint64_t *costs = NULL;
    size_t *choice = NULL, n, i, pass, work = 0, sp = 0;
    unsigned char *overflow = NULL, *active = NULL;
    mulib_egraph__frame *stack = NULL;
    mulib_egraph_tree_t t = { NULL, 0, 0, 0 };
    mulib_egraph_status_t s = MULIB_EGRAPH_OK;
    if (!g || !out || out->nodes || out->count || root >= g->count)
      return MULIB_EGRAPH_INVALID;
    if (g->dirty)
      return MULIB_EGRAPH_DIRTY;
    n = g->count;
    root = mulib_egraph_find (g, root);
    if (n > SIZE_MAX / sizeof (*costs) || n > SIZE_MAX / sizeof (*choice)
        || n > SIZE_MAX / sizeof (*stack)
        || limits.nodes > SIZE_MAX / sizeof (*t.nodes))
      return MULIB_EGRAPH_RESOURCE_LIMIT;
    costs = (uint64_t *)calloc (n, sizeof (*costs));
    choice = (size_t *)malloc (n * sizeof (*choice));
    overflow = (unsigned char *)calloc (n, 1);
    active = (unsigned char *)calloc (n, 1);
    stack = (mulib_egraph__frame *)calloc (n, sizeof (*stack));
    if (!costs || !choice || !overflow || !active || !stack)
      {
        s = MULIB_EGRAPH_NO_MEMORY;
        goto finish;
      }
    for (i = 0; i < n; ++i)
      choice[i] = SIZE_MAX;
    for (pass = 0; pass < n; ++pass)
      {
        int changed = 0;
        for (i = 0; i < n; ++i)
          {
            mulib_egraph_node_t node;
            size_t c;
            uint64_t cost;
            unsigned j;
            int valid = 1;
            if (work == limits.work)
              {
                s = MULIB_EGRAPH_RESOURCE_LIMIT;
                goto finish;
              }
            ++work;
            (void)mulib_egraph_node (g, i, &node, &c);
            cost = fn ? fn (&node, user) : 1;
            for (j = 0; j < node.arity; ++j)
              {
                size_t child = node.child[j];
                if (choice[child] == SIZE_MAX)
                  {
                    if (overflow[child] && !overflow[c])
                      {
                        overflow[c] = 1;
                        changed = 1;
                      }
                    valid = 0;
                    continue;
                  }
                if (costs[child] > UINT64_MAX - cost)
                  {
                    if (!overflow[c])
                      {
                        overflow[c] = 1;
                        changed = 1;
                      }
                    valid = 0;
                  }
                else
                  cost += costs[child];
              }
            if (valid && (choice[c] == SIZE_MAX || cost < costs[c]))
              {
                choice[c] = i;
                costs[c] = cost;
                changed = 1;
              }
          }
        if (!changed)
          break;
      }
    if (choice[root] == SIZE_MAX)
      {
        s = overflow[root] ? MULIB_EGRAPH_OVERFLOW : MULIB_EGRAPH_CYCLE;
        goto finish;
      }
    if (!limits.nodes)
      {
        s = MULIB_EGRAPH_RESOURCE_LIMIT;
        goto finish;
      }
    t.nodes = (mulib_egraph_node_t *)malloc (limits.nodes * sizeof (*t.nodes));
    if (!t.nodes)
      {
        s = MULIB_EGRAPH_NO_MEMORY;
        goto finish;
      }
    t.cost = costs[root];
    stack[0].cls = root;
    active[root] = 1;
    sp = 1;
    while (sp)
      {
        mulib_egraph__frame *f = &stack[sp - 1];
        mulib_egraph_node_t node;
        unsigned j;
        if (work == limits.work)
          {
            s = MULIB_EGRAPH_RESOURCE_LIMIT;
            goto finish;
          }
        ++work;
        (void)mulib_egraph_node (g, choice[f->cls], &node, NULL);
        if (f->next < node.arity)
          {
            size_t c = node.child[f->next++];
            if (active[c] || sp == n)
              {
                s = MULIB_EGRAPH_CYCLE;
                goto finish;
              }
            active[c] = 1;
            stack[sp].cls = c;
            stack[sp++].next = 0;
          }
        else
          {
            size_t id;
            if (t.count == limits.nodes)
              {
                s = MULIB_EGRAPH_RESOURCE_LIMIT;
                goto finish;
              }
            for (j = 0; j < node.arity; ++j)
              node.child[j] = f->child[j];
            id = t.count;
            t.nodes[t.count++] = node;
            active[f->cls] = 0;
            --sp;
            if (sp)
              stack[sp - 1].child[stack[sp - 1].next - 1] = id;
          }
      }
    t.root = t.count - 1;
    *out = t;
    memset (&t, 0, sizeof (t));
  finish:
    free (costs);
    free (choice);
    free (overflow);
    free (active);
    free (stack);
    mulib_egraph_tree_free (&t);
    return s;
  }
  const char *
  mulib_egraph_status_string (mulib_egraph_status_t s)
  {
    switch (s)
      {
      case MULIB_EGRAPH_OK:
        return "ok";
      case MULIB_EGRAPH_INVALID:
        return "invalid graph, ID, node, or output";
      case MULIB_EGRAPH_RESOURCE_LIMIT:
        return "resource limit";
      case MULIB_EGRAPH_NO_MEMORY:
        return "allocation failed";
      case MULIB_EGRAPH_DIRTY:
        return "congruence rebuild required";
      case MULIB_EGRAPH_CYCLE:
        return "no finite acyclic extraction";
      case MULIB_EGRAPH_OVERFLOW:
        return "extraction cost overflow";
      default:
        return "unknown egraph status";
      }
  }
#ifdef __cplusplus
}
#endif
#endif
