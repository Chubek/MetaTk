/** @file mulib-muburg.h
 * @brief Costed bottom-up binary tree grammar instruction selection.
 * Define MULIB_IMPL_MUBURG. No dependencies.
 */
#ifndef MULIB_MUBURG_H
#define MULIB_MUBURG_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** Borrowed node graph. Only child[0..arity) are read; arity is 0, 1 or 2.
   */
  typedef struct mulib_muburg_node
  {
    int opcode;
    unsigned arity;
    size_t child[2];
  } mulib_muburg_node_t;
  /** lhs <- opcode(child_goal[0], child_goal[1]), with an additive nonnegative
   * cost. */
  typedef struct mulib_muburg_rule
  {
    size_t lhs;
    int opcode;
    unsigned arity;
    size_t child_goal[2];
    uint64_t cost;
    unsigned instruction;
  } mulib_muburg_rule_t;
  typedef struct mulib_muburg_limits
  {
    size_t nodes, rules, goals, cells;
  } mulib_muburg_limits_t;
  typedef enum mulib_muburg_status
  {
    MULIB_MUBURG_OK,
    MULIB_MUBURG_INVALID,
    MULIB_MUBURG_CYCLE,
    MULIB_MUBURG_NO_COVER,
    MULIB_MUBURG_OVERFLOW,
    MULIB_MUBURG_RESOURCE_LIMIT,
    MULIB_MUBURG_NO_MEMORY,
    MULIB_MUBURG_STOPPED
  } mulib_muburg_status_t;
  /** Owned DP tables, borrowed immutable input arrays. Zero initialize; do not
   * copy live plans. */
  typedef struct mulib_muburg_plan
  {
    uint64_t *costs;
    size_t *choices;
    const mulib_muburg_node_t *nodes;
    const mulib_muburg_rule_t *rules;
    size_t node_count, rule_count, goal_count, root, goal;
    uint64_t cost;
  } mulib_muburg_plan_t;
  typedef struct mulib_muburg_diagnostic
  {
    mulib_muburg_status_t status;
    size_t node;
  } mulib_muburg_diagnostic_t;
  /** Select the minimum-cost cover. Nonterminals are [0,goals); ties use rule
   * order. Entire graph is checked for cycles. DAGs are costed as expanded
   * trees (no sharing). Rules have one terminal root; no epsilon/chain rules
   * or semantic predicates. Overflowing alternatives are ignored if a finite
   * cover exists.
   */
  mulib_muburg_status_t mulib_muburg_select (
      const mulib_muburg_node_t *nodes, size_t node_count,
      const mulib_muburg_rule_t *rules, size_t rule_count, size_t goals,
      size_t root, size_t goal, mulib_muburg_limits_t limits,
      mulib_muburg_plan_t *output, mulib_muburg_diagnostic_t *diagnostic);
  typedef int (*mulib_muburg_emit_fn) (void *user, size_t node, size_t rule,
                                       unsigned instruction);
  /** Emit selected instructions in child-before-parent order, including
   * repeated DAG uses. emit_limit counts callbacks. A stopped/limited emission
   * has observable partial callbacks.
   */
  mulib_muburg_status_t mulib_muburg_emit (const mulib_muburg_plan_t *plan,
                                           size_t emit_limit,
                                           mulib_muburg_emit_fn callback,
                                           void *user, size_t *emitted);
  /** Release tables, not the borrowed nodes/rules. */
  void mulib_muburg_free (mulib_muburg_plan_t *plan);
  /** Static status description. */
  const char *mulib_muburg_status_string (mulib_muburg_status_t status);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MUBURG) && !defined(MULIB_MUBURG_IMPLEMENTED)
#define MULIB_MUBURG_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_muburg_free (mulib_muburg_plan_t *p)
  {
    if (p)
      {
        free (p->costs);
        free (p->choices);
        memset (p, 0, sizeof (*p));
      }
  }
  mulib_muburg_status_t
  mulib_muburg_select (const mulib_muburg_node_t *nodes, size_t n,
                       const mulib_muburg_rule_t *rules, size_t nr, size_t ng,
                       size_t root, size_t goal, mulib_muburg_limits_t limits,
                       mulib_muburg_plan_t *out, mulib_muburg_diagnostic_t *d)
  {
    mulib_muburg_plan_t p;
    mulib_muburg_status_t s = MULIB_MUBURG_OK;
    unsigned char *done = NULL, *over = NULL;
    size_t k = 0, j, pos, cells;
    memset (&p, 0, sizeof (p));
    if (!out || out->costs || out->choices || !nodes || !n || (!rules && nr)
        || !ng || root >= n || goal >= ng)
      {
        s = MULIB_MUBURG_INVALID;
        goto finish;
      }
    if (n > limits.nodes || nr > limits.rules || ng > limits.goals
        || n > SIZE_MAX / ng || nr > SIZE_MAX / sizeof (*rules)
        || n > SIZE_MAX / sizeof (*nodes))
      {
        s = MULIB_MUBURG_RESOURCE_LIMIT;
        goto finish;
      }
    cells = n * ng;
    if (cells > limits.cells || cells > SIZE_MAX / sizeof (*p.choices)
        || cells > SIZE_MAX / sizeof (*p.costs))
      {
        s = MULIB_MUBURG_RESOURCE_LIMIT;
        goto finish;
      }
    for (k = 0; k < n; ++k)
      {
        if (nodes[k].arity > 2)
          {
            s = MULIB_MUBURG_INVALID;
            goto finish;
          }
        for (j = 0; j < nodes[k].arity; ++j)
          if (nodes[k].child[j] >= n)
            {
              s = MULIB_MUBURG_INVALID;
              goto finish;
            }
      }
    for (k = 0; k < nr; ++k)
      {
        if (rules[k].arity > 2 || rules[k].lhs >= ng)
          {
            s = MULIB_MUBURG_INVALID;
            goto finish;
          }
        for (j = 0; j < rules[k].arity; ++j)
          if (rules[k].child_goal[j] >= ng)
            {
              s = MULIB_MUBURG_INVALID;
              goto finish;
            }
      }
    p.costs = (uint64_t *)calloc (cells, sizeof (*p.costs));
    p.choices = (size_t *)malloc (cells * sizeof (*p.choices));
    done = (unsigned char *)calloc (n, 1);
    over = (unsigned char *)calloc (cells, 1);
    if (!p.costs || !p.choices || !done || !over)
      {
        s = MULIB_MUBURG_NO_MEMORY;
        goto finish;
      }
    for (k = 0; k < cells; ++k)
      p.choices[k] = SIZE_MAX;
    for (pos = 0; pos < n; ++pos)
      {
        for (k = 0; k < n; ++k)
          if (!done[k])
            {
              for (j = 0; j < nodes[k].arity && done[nodes[k].child[j]]; ++j)
                {
                }
              if (j == nodes[k].arity)
                break;
            }
        if (k == n)
          {
            s = MULIB_MUBURG_CYCLE;
            goto finish;
          }
        for (j = 0; j < nr; ++j)
          if (rules[j].opcode == nodes[k].opcode
              && rules[j].arity == nodes[k].arity)
            {
              uint64_t cost = rules[j].cost;
              size_t a, cell = k * ng + rules[j].lhs;
              int valid = 1, exceeds = 0;
              for (a = 0; a < rules[j].arity; ++a)
                {
                  size_t c = nodes[k].child[a] * ng + rules[j].child_goal[a];
                  if (p.choices[c] == SIZE_MAX)
                    {
                      if (over[c])
                        exceeds = 1;
                      else
                        valid = 0;
                    }
                  else if (!exceeds)
                    {
                      if (p.costs[c] > UINT64_MAX - cost)
                        exceeds = 1;
                      else
                        cost += p.costs[c];
                    }
                }
              if (valid && exceeds)
                over[cell] = 1;
              if (valid && !exceeds
                  && (p.choices[cell] == SIZE_MAX || cost < p.costs[cell]))
                {
                  p.choices[cell] = j;
                  p.costs[cell] = cost;
                }
            }
        done[k] = 1;
      }
    k = root;
    if (p.choices[root * ng + goal] == SIZE_MAX)
      {
        s = over[root * ng + goal] ? MULIB_MUBURG_OVERFLOW
                                   : MULIB_MUBURG_NO_COVER;
        goto finish;
      }
    p.nodes = nodes;
    p.rules = rules;
    p.node_count = n;
    p.rule_count = nr;
    p.goal_count = ng;
    p.root = root;
    p.goal = goal;
    p.cost = p.costs[root * ng + goal];
    *out = p;
    memset (&p, 0, sizeof (p));
  finish:
    free (done);
    free (over);
    mulib_muburg_free (&p);
    if (d)
      {
        d->status = s;
        d->node = k;
      }
    return s;
  }
  typedef struct mulib_muburg__frame
  {
    size_t node, goal;
    unsigned next;
  } mulib_muburg__frame;
  mulib_muburg_status_t
  mulib_muburg_emit (const mulib_muburg_plan_t *p, size_t limit,
                     mulib_muburg_emit_fn fn, void *user, size_t *emitted)
  {
    mulib_muburg__frame *stack;
    size_t sp = 0, count = 0;
    mulib_muburg_status_t s = MULIB_MUBURG_OK;
    if (emitted)
      *emitted = 0;
    if (!p || !p->choices || !p->nodes || !p->rules || !fn || !p->node_count
        || !p->goal_count || p->root >= p->node_count
        || p->goal >= p->goal_count)
      return MULIB_MUBURG_INVALID;
    if (p->node_count > SIZE_MAX / sizeof (*stack)
        || p->node_count > SIZE_MAX / sizeof (*p->nodes)
        || p->rule_count > SIZE_MAX / sizeof (*p->rules)
        || p->node_count > SIZE_MAX / p->goal_count
        || p->node_count * p->goal_count > SIZE_MAX / sizeof (*p->choices))
      return MULIB_MUBURG_RESOURCE_LIMIT;
    stack = (mulib_muburg__frame *)calloc (p->node_count, sizeof (*stack));
    if (!stack)
      return MULIB_MUBURG_NO_MEMORY;
    stack[sp].node = p->root;
    stack[sp++].goal = p->goal;
    while (sp)
      {
        mulib_muburg__frame *f = &stack[sp - 1];
        size_t r = p->choices[f->node * p->goal_count + f->goal];
        if (r >= p->rule_count)
          {
            s = MULIB_MUBURG_INVALID;
            break;
          }
        if (p->rules[r].arity > 2
            || p->rules[r].arity != p->nodes[f->node].arity
            || p->rules[r].opcode != p->nodes[f->node].opcode
            || p->rules[r].lhs != f->goal)
          {
            s = MULIB_MUBURG_INVALID;
            break;
          }
        if (f->next < p->rules[r].arity)
          {
            unsigned child = f->next++;
            if (p->nodes[f->node].child[child] >= p->node_count
                || p->rules[r].child_goal[child] >= p->goal_count)
              {
                s = MULIB_MUBURG_INVALID;
                break;
              }
            if (sp == p->node_count)
              {
                s = MULIB_MUBURG_CYCLE;
                break;
              }
            stack[sp].node = p->nodes[f->node].child[child];
            stack[sp].goal = p->rules[r].child_goal[child];
            stack[sp++].next = 0;
          }
        else
          {
            if (count == limit)
              {
                s = MULIB_MUBURG_RESOURCE_LIMIT;
                break;
              }
            ++count;
            if (!fn (user, f->node, r, p->rules[r].instruction))
              {
                s = MULIB_MUBURG_STOPPED;
                break;
              }
            --sp;
          }
      }
    free (stack);
    if (emitted)
      *emitted = count;
    return s;
  }
  const char *
  mulib_muburg_status_string (mulib_muburg_status_t s)
  {
    switch (s)
      {
      case MULIB_MUBURG_OK:
        return "ok";
      case MULIB_MUBURG_INVALID:
        return "invalid tree, grammar, or plan";
      case MULIB_MUBURG_CYCLE:
        return "cyclic node graph";
      case MULIB_MUBURG_NO_COVER:
        return "no grammar cover";
      case MULIB_MUBURG_OVERFLOW:
        return "cost overflow prevents a cover";
      case MULIB_MUBURG_RESOURCE_LIMIT:
        return "resource limit";
      case MULIB_MUBURG_NO_MEMORY:
        return "allocation failed";
      case MULIB_MUBURG_STOPPED:
        return "callback stopped";
      default:
        return "unknown selector status";
      }
  }
#ifdef __cplusplus
}
#endif
#endif
