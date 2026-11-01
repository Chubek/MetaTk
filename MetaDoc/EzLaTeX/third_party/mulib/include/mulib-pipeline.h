/**
 * @file mulib-pipeline.h
 * @brief Item-parallel, ordered-stage pipeline with deterministic buffer
 * disposal. Supply MULIB_IMPL_PIPELINE, MULIB_IMPL_PARALLEL and
 * MULIB_IMPL_TASKPOOL separately.
 */
#ifndef MULIB_PIPELINE_H
#define MULIB_PIPELINE_H
#include "mulib-parallel.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_pipeline_item
  {
    void *data;
    size_t size;
  } mulib_pipeline_item_t;
  typedef int (*mulib_pipeline_stage_fn) (mulib_pipeline_item_t *item,
                                          void *context);
  typedef void (*mulib_pipeline_release_fn) (mulib_pipeline_item_t *item,
                                             void *context);
  typedef struct mulib_pipeline_stage
  {
    mulib_pipeline_stage_fn fn;
    void *context;
  } mulib_pipeline_stage_t;
  /** Validate then consume every item. Stages run in order per item, in
   * parallel across items. release runs once per item on the calling thread
   * after all work, including failures/cancellation, then data/size are
   * cleared. Before validation succeeds, ownership stays with caller. Stage
   * replacements must dispose old data. On allocation/submission failure after
   * validation, items are still consumed. */
  int mulib_pipeline_run (mulib_taskpool_t *pool,
                          const mulib_pipeline_stage_t *stages,
                          size_t stage_count, mulib_pipeline_item_t *items,
                          size_t count, mulib_pipeline_release_fn release,
                          void *release_context,
                          const mulib_task_cancel_t *cancel, size_t *completed,
                          int *error);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_PIPELINE) && !defined(MULIB_PIPELINE_IMPLEMENTED)
#define MULIB_PIPELINE_IMPLEMENTED
#include <stdatomic.h>
#include <stdint.h>
typedef struct mulib_pipeline_context
{
  const mulib_pipeline_stage_t *stages;
  size_t stage_count;
  mulib_pipeline_item_t *items;
  const mulib_task_cancel_t *cancel;
  atomic_size_t completed;
} mulib_pipeline_context_t;
static int
mulib_pipeline_one_ (size_t index, void *arg)
{
  mulib_pipeline_context_t *c = (mulib_pipeline_context_t *)arg;
  size_t i;
  for (i = 0; i < c->stage_count; ++i)
    {
      int rc;
      if (mulib_task_cancel_requested (c->cancel))
        return 0;
      rc = c->stages[i].fn (&c->items[index], c->stages[i].context);
      if (rc)
        return rc;
      if (!c->items[index].data && c->items[index].size)
        return MULIB_TASK_INVALID;
    }
  (void)atomic_fetch_add (&c->completed, 1);
  return 0;
}
int
mulib_pipeline_run (mulib_taskpool_t *p, const mulib_pipeline_stage_t *stages,
                    size_t ns, mulib_pipeline_item_t *items, size_t count,
                    mulib_pipeline_release_fn release, void *release_arg,
                    const mulib_task_cancel_t *cancel, size_t *completed,
                    int *error)
{
  mulib_pipeline_context_t c;
  size_t i;
  int rc;
  if (completed)
    *completed = 0;
  if (error)
    *error = 0;
  if (!p || !release || (ns && !stages) || (count && !items)
      || ns > SIZE_MAX / sizeof (*stages)
      || count > SIZE_MAX / sizeof (*items))
    return MULIB_TASK_INVALID;
  for (i = 0; i < ns; ++i)
    if (!stages[i].fn)
      return MULIB_TASK_INVALID;
  for (i = 0; i < count; ++i)
    if (!items[i].data && items[i].size)
      return MULIB_TASK_INVALID;
  c.stages = stages;
  c.stage_count = ns;
  c.items = items;
  c.cancel = cancel;
  atomic_init (&c.completed, 0);
  rc = mulib_parallel_for (p, 0, count, 1, mulib_pipeline_one_, &c, cancel,
                           error);
  if (completed)
    *completed = atomic_load (&c.completed);
  for (i = 0; i < count; ++i)
    {
      release (&items[i], release_arg);
      items[i].data = NULL;
      items[i].size = 0;
    }
  return rc;
}
#endif
