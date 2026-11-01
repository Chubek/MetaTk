/**
 * @file mulib-taskdag.h
 * @brief Bounded dependency graph, cycle validation and pool-backed wave
 * execution. Define MULIB_IMPL_TASKDAG once and provide MULIB_IMPL_TASKPOOL
 * separately.
 */
#ifndef MULIB_TASKDAG_H
#define MULIB_TASKDAG_H
#include "mulib-taskpool.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_taskdag mulib_taskdag_t;
  typedef int (*mulib_taskdag_fn) (void *context);
  enum
  {
    MULIB_TASKDAG_PENDING,
    MULIB_TASKDAG_RUNNING,
    MULIB_TASKDAG_SUCCEEDED,
    MULIB_TASKDAG_FAILED,
    MULIB_TASKDAG_BLOCKED,
    MULIB_TASKDAG_CANCELLED
  };
  /** Allocate fixed node/edge capacities; zero edges is supported. */
  int mulib_taskdag_create (mulib_taskdag_t **out, size_t nodes, size_t edges);
  /** Borrow a callback/context and return its stable node ID. */
  int mulib_taskdag_add (mulib_taskdag_t *dag, mulib_taskdag_fn fn,
                         void *context, size_t *id);
  /** Add before -> after. Duplicate edges are harmless; self-edges are cycles.
   */
  int mulib_taskdag_depend (mulib_taskdag_t *dag, size_t before, size_t after);
  /** Write a topological order; return CYCLE before executing any callbacks.
   */
  int mulib_taskdag_order (const mulib_taskdag_t *dag, size_t *order,
                           size_t capacity);
  /** Run dependency frontiers in parallel; failed descendants are BLOCKED.
   * Caller must not be a pool worker; use an otherwise exclusively controlled
   * pool. */
  int mulib_taskdag_run (mulib_taskdag_t *dag, mulib_taskpool_t *pool,
                         const mulib_task_cancel_t *cancel);
  /** Query after run. result is the original callback code (zero for skipped
   * jobs). */
  int mulib_taskdag_result (const mulib_taskdag_t *dag, size_t id, int *state,
                            int *result);
  /** Return number of registered nodes (zero for NULL). */
  size_t mulib_taskdag_count (const mulib_taskdag_t *dag);
  /** Free graph storage, never callback contexts. No concurrent use. */
  void mulib_taskdag_destroy (mulib_taskdag_t *dag);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_TASKDAG) && !defined(MULIB_TASKDAG_IMPLEMENTED)
#define MULIB_TASKDAG_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
typedef struct mulib_taskdag_node
{
  mulib_taskdag_fn fn;
  void *context;
  int state, result;
} mulib_taskdag_node_t;
typedef struct mulib_taskdag_edge
{
  size_t before, after;
} mulib_taskdag_edge_t;
struct mulib_taskdag
{
  mulib_taskdag_node_t *nodes;
  mulib_taskdag_edge_t *edges;
  size_t count, edge_count, capacity, edge_capacity;
};
typedef struct mulib_taskdag_job
{
  mulib_taskdag_node_t *node;
  const mulib_task_cancel_t *cancel;
  int state, result;
} mulib_taskdag_job_t;
int
mulib_taskdag_create (mulib_taskdag_t **out, size_t n, size_t e)
{
  mulib_taskdag_t *d;
  if (!out || !n || n > SIZE_MAX / sizeof (mulib_taskdag_node_t)
      || e > SIZE_MAX / sizeof (mulib_taskdag_edge_t))
    return MULIB_TASK_INVALID;
  *out = NULL;
  d = (mulib_taskdag_t *)calloc (1, sizeof (*d));
  if (!d)
    return MULIB_TASK_NOMEM;
  d->nodes = (mulib_taskdag_node_t *)calloc (n, sizeof (*d->nodes));
  d->edges
      = e ? (mulib_taskdag_edge_t *)malloc (e * sizeof (*d->edges)) : NULL;
  if (!d->nodes || (e && !d->edges))
    {
      mulib_taskdag_destroy (d);
      return MULIB_TASK_NOMEM;
    }
  d->capacity = n;
  d->edge_capacity = e;
  *out = d;
  return MULIB_TASK_OK;
}
int
mulib_taskdag_add (mulib_taskdag_t *d, mulib_taskdag_fn fn, void *arg,
                   size_t *id)
{
  mulib_taskdag_node_t *node;
  if (!d || !fn || !id)
    return MULIB_TASK_INVALID;
  if (d->count == d->capacity)
    return MULIB_TASK_FULL;
  *id = d->count++;
  node = &d->nodes[*id];
  node->fn = fn;
  node->context = arg;
  node->state = MULIB_TASKDAG_PENDING;
  node->result = 0;
  return MULIB_TASK_OK;
}
int
mulib_taskdag_depend (mulib_taskdag_t *d, size_t a, size_t b)
{
  size_t i;
  if (!d || a >= d->count || b >= d->count)
    return MULIB_TASK_INVALID;
  for (i = 0; i < d->edge_count; ++i)
    if (d->edges[i].before == a && d->edges[i].after == b)
      return MULIB_TASK_OK;
  if (d->edge_count == d->edge_capacity)
    return MULIB_TASK_FULL;
  d->edges[d->edge_count].before = a;
  d->edges[d->edge_count++].after = b;
  return MULIB_TASK_OK;
}
int
mulib_taskdag_order (const mulib_taskdag_t *d, size_t *order, size_t cap)
{
  size_t *degree, i, head = 0, tail = 0;
  if (!d || cap < d->count || (!order && d->count))
    return MULIB_TASK_INVALID;
  if (!d->count)
    return MULIB_TASK_OK;
  if (d->count > SIZE_MAX / sizeof (*degree))
    return MULIB_TASK_INVALID;
  degree = (size_t *)calloc (d->count, sizeof (*degree));
  if (!degree)
    return MULIB_TASK_NOMEM;
  for (i = 0; i < d->edge_count; ++i)
    ++degree[d->edges[i].after];
  for (i = 0; i < d->count; ++i)
    if (!degree[i])
      order[tail++] = i;
  while (head < tail)
    {
      size_t node = order[head++];
      for (i = 0; i < d->edge_count; ++i)
        if (d->edges[i].before == node && !--degree[d->edges[i].after])
          order[tail++] = d->edges[i].after;
    }
  free (degree);
  return tail == d->count ? MULIB_TASK_OK : MULIB_TASK_CYCLE;
}
static void
mulib_taskdag_execute_ (void *arg)
{
  mulib_taskdag_job_t *j = (mulib_taskdag_job_t *)arg;
  if (mulib_task_cancel_requested (j->cancel))
    {
      j->state = MULIB_TASKDAG_CANCELLED;
      return;
    }
  j->result = j->node->fn (j->node->context);
  j->state = j->result ? MULIB_TASKDAG_FAILED : MULIB_TASKDAG_SUCCEEDED;
}
static void
mulib_taskdag_cancel_ (void *arg)
{
  ((mulib_taskdag_job_t *)arg)->state = MULIB_TASKDAG_CANCELLED;
}
int
mulib_taskdag_run (mulib_taskdag_t *d, mulib_taskpool_t *p,
                   const mulib_task_cancel_t *cancel)
{
  size_t *order, i, remaining;
  mulib_taskdag_job_t *jobs;
  int rc, overall = MULIB_TASK_OK;
  if (!d || !p)
    return MULIB_TASK_INVALID;
  /* No borrowed job storage may be submitted from a same-pool callback. */
  rc = mulib_taskpool_wait (p);
  if (rc)
    return rc;
  for (i = 0; i < d->count; ++i)
    {
      d->nodes[i].state = MULIB_TASKDAG_PENDING;
      d->nodes[i].result = 0;
    }
  if (!d->count)
    return MULIB_TASK_OK;
  if (d->count > SIZE_MAX / sizeof (*order)
      || d->count > SIZE_MAX / sizeof (*jobs))
    return MULIB_TASK_INVALID;
  order = (size_t *)malloc (d->count * sizeof (*order));
  if (!order)
    return MULIB_TASK_NOMEM;
  rc = mulib_taskdag_order (d, order, d->count);
  free (order);
  if (rc)
    return rc;
  jobs = (mulib_taskdag_job_t *)calloc (d->count, sizeof (*jobs));
  if (!jobs)
    return MULIB_TASK_NOMEM;
  remaining = d->count;
  while (remaining)
    {
      for (i = 0; i < d->count; ++i)
        {
          size_t e;
          int ready = 1, blocked = 0;
          mulib_taskdag_node_t *n = &d->nodes[i];
          if (n->state != MULIB_TASKDAG_PENDING)
            continue;
          for (e = 0; e < d->edge_count; ++e)
            if (d->edges[e].after == i)
              {
                int s = d->nodes[d->edges[e].before].state;
                if (s == MULIB_TASKDAG_PENDING || s == MULIB_TASKDAG_RUNNING)
                  ready = 0;
                else if (s != MULIB_TASKDAG_SUCCEEDED)
                  blocked = 1;
              }
          if (!ready)
            continue;
          if (mulib_task_cancel_requested (cancel) || blocked)
            {
              n->state = mulib_task_cancel_requested (cancel)
                             ? MULIB_TASKDAG_CANCELLED
                             : MULIB_TASKDAG_BLOCKED;
              if (!overall)
                overall = n->state == MULIB_TASKDAG_CANCELLED
                              ? MULIB_TASK_CANCELLED
                              : MULIB_TASK_FAILED;
              --remaining;
              continue;
            }
          jobs[i].node = n;
          jobs[i].cancel = cancel;
          jobs[i].result = 0;
          jobs[i].state = MULIB_TASKDAG_CANCELLED;
          n->state = MULIB_TASKDAG_RUNNING;
          do
            {
              rc = mulib_taskpool_submit (p, mulib_taskdag_execute_, &jobs[i],
                                          mulib_taskdag_cancel_);
              if (rc == MULIB_TASK_FULL)
                (void)mulib_taskpool_wait (p);
            }
          while (rc == MULIB_TASK_FULL);
          if (rc)
            {
              n->state = MULIB_TASKDAG_CANCELLED;
              --remaining;
              if (!overall)
                overall = rc;
            }
        }
      (void)mulib_taskpool_wait (p);
      for (i = 0; i < d->count; ++i)
        if (d->nodes[i].state == MULIB_TASKDAG_RUNNING)
          {
            d->nodes[i].state = jobs[i].state;
            d->nodes[i].result = jobs[i].result;
            if (!overall && jobs[i].state != MULIB_TASKDAG_SUCCEEDED)
              overall = jobs[i].state == MULIB_TASKDAG_CANCELLED
                            ? MULIB_TASK_CANCELLED
                            : MULIB_TASK_FAILED;
            --remaining;
          }
    }
  free (jobs);
  return overall;
}
int
mulib_taskdag_result (const mulib_taskdag_t *d, size_t id, int *state,
                      int *result)
{
  if (!d || id >= d->count || (!state && !result))
    return MULIB_TASK_INVALID;
  if (state)
    *state = d->nodes[id].state;
  if (result)
    *result = d->nodes[id].result;
  return MULIB_TASK_OK;
}
size_t
mulib_taskdag_count (const mulib_taskdag_t *d)
{
  return d ? d->count : 0;
}
void
mulib_taskdag_destroy (mulib_taskdag_t *d)
{
  if (d)
    {
      free (d->nodes);
      free (d->edges);
      free (d);
    }
}
#endif
