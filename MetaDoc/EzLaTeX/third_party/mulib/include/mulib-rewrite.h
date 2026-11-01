/** @file mulib-rewrite.h
 * @brief Bounded e-matching and equality saturation over mulib-egraph.
 * Define MULIB_IMPL_REWRITE; explicitly implement EGRAPH as well.
 */
#ifndef MULIB_REWRITE_H
#define MULIB_REWRITE_H
#include "mulib-egraph.h"
#ifdef __cplusplus
extern "C"
{
#endif
#define MULIB_REWRITE_PATTERN_LIMIT 64
#define MULIB_REWRITE_VARIABLE_LIMIT 32
  typedef enum mulib_rewrite_kind
  {
    MULIB_REWRITE_NODE,
    MULIB_REWRITE_VARIABLE
  } mulib_rewrite_kind_t;
  /** A postorder pattern tree. Root is last; children must precede parent, and
   * each nonroot record has exactly one parent. Repeated variable numbers
   * express equality constraints; use separate VARIABLE records for separate
   * occurrences. NODE matches opcode, literal and arity exactly (children
   * match classes).
   */
  typedef struct mulib_rewrite_pattern
  {
    mulib_rewrite_kind_t kind;
    unsigned variable, opcode, arity;
    int64_t literal;
    size_t child[2];
  } mulib_rewrite_pattern_t;
  typedef struct mulib_rewrite_rule
  {
    const mulib_rewrite_pattern_t *lhs, *rhs;
    size_t lhs_count, rhs_count;
  } mulib_rewrite_rule_t;
  typedef struct mulib_rewrite_limits
  {
    size_t rules, iterations, matches, work;
  } mulib_rewrite_limits_t;
  typedef enum mulib_rewrite_status
  {
    MULIB_REWRITE_OK,
    MULIB_REWRITE_INVALID,
    MULIB_REWRITE_ITERATION_LIMIT,
    MULIB_REWRITE_MATCH_LIMIT,
    MULIB_REWRITE_WORK_LIMIT,
    MULIB_REWRITE_EGRAPH_ERROR,
    MULIB_REWRITE_RULE_LIMIT
  } mulib_rewrite_status_t;
  typedef struct mulib_rewrite_result
  {
    mulib_rewrite_status_t status;
    mulib_egraph_status_t graph_status;
    size_t iterations, matches, applications, work, rule, class_id;
  } mulib_rewrite_result_t;
  /** Enumerate matching alternatives with backtracking and repeated-variable
   * checks; instantiate RHS then union with the matched class and rebuild
   * congruence. RHS variables must occur on LHS. All rules are validated
   * before mutation. Return OK only when a complete iteration adds no
   * nodes/unions. Every operation is bounded; limits are actual zeros, not
   * default/unlimited sentinels. Mutations are NOT rolled back on
   * failure/limits. A rebuild-limit failure can leave graph dirty. No
   * arithmetic semantics/side conditions are inferred.
   */
  mulib_rewrite_status_t mulib_rewrite_run (mulib_egraph_t *graph,
                                            const mulib_rewrite_rule_t *rules,
                                            size_t rule_count,
                                            mulib_rewrite_limits_t limits,
                                            mulib_rewrite_result_t *result);
  /** Static status description. */
  const char *mulib_rewrite_status_string (mulib_rewrite_status_t status);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_REWRITE) && !defined(MULIB_REWRITE_IMPLEMENTED)
#define MULIB_REWRITE_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_rewrite__pattern (const mulib_rewrite_pattern_t *p, size_t n,
                          uint32_t *variables)
  {
    unsigned parents[MULIB_REWRITE_PATTERN_LIMIT] = { 0 };
    size_t i;
    uint32_t vars = 0;
    if (!p || !n || n > MULIB_REWRITE_PATTERN_LIMIT)
      return 0;
    for (i = 0; i < n; ++i)
      {
        unsigned j;
        if (p[i].kind == MULIB_REWRITE_VARIABLE)
          {
            if (p[i].variable >= MULIB_REWRITE_VARIABLE_LIMIT || p[i].arity)
              return 0;
            vars |= UINT32_C (1) << p[i].variable;
          }
        else if (p[i].kind == MULIB_REWRITE_NODE && p[i].arity <= 2)
          {
            for (j = 0; j < p[i].arity; ++j)
              if (p[i].child[j] >= i || ++parents[p[i].child[j]] != 1)
                return 0;
          }
        else
          return 0;
      }
    for (i = 0; i + 1 < n; ++i)
      if (parents[i] != 1)
        return 0;
    *variables = vars;
    return 1;
  }
  typedef struct mulib_rewrite__task
  {
    size_t pattern, cls;
  } mulib_rewrite__task;
  typedef struct mulib_rewrite__state
  {
    mulib_rewrite__task tasks[MULIB_REWRITE_PATTERN_LIMIT];
    size_t bindings[MULIB_REWRITE_VARIABLE_LIMIT], count;
    uint32_t bound;
  } mulib_rewrite__state;
  typedef struct mulib_rewrite__context
  {
    mulib_egraph_t *graph;
    const mulib_rewrite_rule_t *rule;
    mulib_rewrite_limits_t limits;
    mulib_rewrite_result_t result;
  } mulib_rewrite__context;
  static int
  mulib_rewrite__work (mulib_rewrite__context *c)
  {
    if (c->result.work == c->limits.work)
      {
        c->result.status = MULIB_REWRITE_WORK_LIMIT;
        return 0;
      }
    ++c->result.work;
    return 1;
  }
  static int
  mulib_rewrite__rebuild (mulib_rewrite__context *c)
  {
    size_t used = 0;
    mulib_egraph_status_t s = mulib_egraph_rebuild (
        c->graph, c->limits.work - c->result.work, &used);
    c->result.work += used;
    if (s == MULIB_EGRAPH_OK)
      return 1;
    c->result.graph_status = s;
    c->result.status = s == MULIB_EGRAPH_RESOURCE_LIMIT
                           ? MULIB_REWRITE_WORK_LIMIT
                           : MULIB_REWRITE_EGRAPH_ERROR;
    return 0;
  }
  static int
  mulib_rewrite__apply (mulib_rewrite__context *c,
                        const mulib_rewrite__state *state)
  {
    size_t values[MULIB_REWRITE_PATTERN_LIMIT], i;
    int changed;
    if (c->result.matches == c->limits.matches)
      {
        c->result.status = MULIB_REWRITE_MATCH_LIMIT;
        return 0;
      }
    ++c->result.matches;
    for (i = 0; i < c->rule->rhs_count; ++i)
      {
        const mulib_rewrite_pattern_t *p = &c->rule->rhs[i];
        if (!mulib_rewrite__work (c))
          return 0;
        if (p->kind == MULIB_REWRITE_VARIABLE)
          values[i]
              = mulib_egraph_find (c->graph, state->bindings[p->variable]);
        else
          {
            mulib_egraph_node_t n;
            unsigned j;
            n.opcode = p->opcode;
            n.arity = p->arity;
            n.literal = p->literal;
            n.child[0] = n.child[1] = MULIB_EGRAPH_NONE;
            for (j = 0; j < n.arity; ++j)
              n.child[j] = values[p->child[j]];
            c->result.graph_status
                = mulib_egraph_add (c->graph, n, &values[i]);
            if (c->result.graph_status != MULIB_EGRAPH_OK)
              {
                c->result.status = MULIB_REWRITE_EGRAPH_ERROR;
                return 0;
              }
          }
      }
    c->result.graph_status
        = mulib_egraph_union (c->graph, c->result.class_id,
                              values[c->rule->rhs_count - 1], &changed);
    if (c->result.graph_status != MULIB_EGRAPH_OK)
      {
        c->result.status = MULIB_REWRITE_EGRAPH_ERROR;
        return 0;
      }
    if (changed)
      ++c->result.applications;
    return mulib_rewrite__rebuild (c);
  }
  static int
  mulib_rewrite__match (mulib_rewrite__context *c,
                        const mulib_rewrite__state *state)
  {
    mulib_rewrite__state next;
    mulib_rewrite__task task;
    const mulib_rewrite_pattern_t *p;
    size_t i, n;
    if (!mulib_rewrite__work (c))
      return 0;
    if (!state->count)
      return mulib_rewrite__apply (c, state);
    next = *state;
    task = next.tasks[--next.count];
    task.cls = mulib_egraph_find (c->graph, task.cls);
    p = &c->rule->lhs[task.pattern];
    if (p->kind == MULIB_REWRITE_VARIABLE)
      {
        uint32_t bit = UINT32_C (1) << p->variable;
        if ((next.bound & bit)
            && mulib_egraph_find (c->graph, next.bindings[p->variable])
                   != task.cls)
          return 1;
        next.bound |= bit;
        next.bindings[p->variable] = task.cls;
        return mulib_rewrite__match (c, &next);
      }
    n = mulib_egraph_count (c->graph);
    for (i = 0; i < n; ++i)
      {
        mulib_egraph_node_t node;
        size_t cls;
        unsigned j;
        if (!mulib_rewrite__work (c))
          return 0;
        (void)mulib_egraph_node (c->graph, i, &node, &cls);
        if (cls != mulib_egraph_find (c->graph, task.cls)
            || node.opcode != p->opcode || node.arity != p->arity
            || node.literal != p->literal)
          continue;
        next = *state;
        --next.count;
        for (j = p->arity; j-- > 0;)
          {
            next.tasks[next.count].pattern = p->child[j];
            next.tasks[next.count++].cls = node.child[j];
          }
        if (!mulib_rewrite__match (c, &next))
          return 0;
      }
    return 1;
  }
  mulib_rewrite_status_t
  mulib_rewrite_run (mulib_egraph_t *g, const mulib_rewrite_rule_t *rules,
                     size_t nr, mulib_rewrite_limits_t limits,
                     mulib_rewrite_result_t *out)
  {
    mulib_rewrite__context c;
    size_t r, iteration;
    memset (&c, 0, sizeof (c));
    c.graph = g;
    c.limits = limits;
    if (!g || (!rules && nr))
      {
        c.result.status = MULIB_REWRITE_INVALID;
        goto finish;
      }
    if (nr > limits.rules || nr > SIZE_MAX / sizeof (*rules))
      {
        c.result.status = MULIB_REWRITE_RULE_LIMIT;
        goto finish;
      }
    for (r = 0; r < nr; ++r)
      {
        uint32_t lhs, rhs;
        c.result.rule = r;
        if (!mulib_rewrite__pattern (rules[r].lhs, rules[r].lhs_count, &lhs)
            || !mulib_rewrite__pattern (rules[r].rhs, rules[r].rhs_count, &rhs)
            || (rhs & ~lhs))
          {
            c.result.status = MULIB_REWRITE_INVALID;
            goto finish;
          }
      }
    if (!mulib_rewrite__rebuild (&c))
      goto finish;
    if (!nr)
      goto finish;
    for (iteration = 0; iteration < limits.iterations; ++iteration)
      {
        size_t revision = mulib_egraph_revision (g),
               n = mulib_egraph_count (g), cls;
        ++c.result.iterations;
        for (r = 0; r < nr; ++r)
          {
            c.rule = &rules[r];
            c.result.rule = r;
            for (cls = 0; cls < n; ++cls)
              if (mulib_egraph_find (g, cls) == cls)
                {
                  mulib_rewrite__state state;
                  memset (&state, 0, sizeof (state));
                  state.count = 1;
                  state.tasks[0].pattern = c.rule->lhs_count - 1;
                  state.tasks[0].cls = cls;
                  c.result.class_id = cls;
                  if (!mulib_rewrite__match (&c, &state))
                    goto finish;
                }
          }
        if (revision == mulib_egraph_revision (g))
          goto finish;
      }
    c.result.status = MULIB_REWRITE_ITERATION_LIMIT;
  finish:
    if (out)
      *out = c.result;
    return c.result.status;
  }
  const char *
  mulib_rewrite_status_string (mulib_rewrite_status_t s)
  {
    switch (s)
      {
      case MULIB_REWRITE_OK:
        return "saturated";
      case MULIB_REWRITE_INVALID:
        return "invalid rule, pattern, or limits";
      case MULIB_REWRITE_ITERATION_LIMIT:
        return "iteration limit before proven saturation";
      case MULIB_REWRITE_MATCH_LIMIT:
        return "match limit";
      case MULIB_REWRITE_WORK_LIMIT:
        return "work limit";
      case MULIB_REWRITE_EGRAPH_ERROR:
        return "egraph operation failed; inspect graph_status";
      case MULIB_REWRITE_RULE_LIMIT:
        return "rule count limit";
      default:
        return "unknown rewrite status";
      }
  }
#ifdef __cplusplus
}
#endif
#endif
