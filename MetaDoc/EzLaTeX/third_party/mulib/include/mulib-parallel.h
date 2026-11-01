/**
 * @file mulib-parallel.h
 * @brief Pool-backed, cooperative parallel for and fixed-stride map.
 * Define MULIB_IMPL_PARALLEL and separately supply MULIB_IMPL_TASKPOOL.
 */
#ifndef MULIB_PARALLEL_H
#define MULIB_PARALLEL_H
#include "mulib-taskpool.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef int (*mulib_parallel_fn) (size_t index, void *context);
  typedef int (*mulib_parallel_map_fn) (size_t index, const void *input,
                                        void *output, void *context);
  /** Execute [begin,end) in chunks of grain. Failure stops unstarted
   * iterations; error receives one original nonzero callback code, or zero.
   * Partial results remain. Do not call from this pool's callbacks or share
   * pool lifecycle control. */
  int mulib_parallel_for (mulib_taskpool_t *pool, size_t begin, size_t end,
                          size_t grain, mulib_parallel_fn fn, void *context,
                          const mulib_task_cancel_t *cancel, int *error);
  /** Fixed-stride byte map. Buffers must not overlap except identical in-place
   * input/output with equal strides. Each callback exclusively owns its output
   * slot. */
  int mulib_parallel_map (mulib_taskpool_t *pool, const void *input,
                          size_t input_stride, void *output,
                          size_t output_stride, size_t count, size_t grain,
                          mulib_parallel_map_fn fn, void *context,
                          const mulib_task_cancel_t *cancel, int *error);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_PARALLEL) && !defined(MULIB_PARALLEL_IMPLEMENTED)
#define MULIB_PARALLEL_IMPLEMENTED
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
typedef struct mulib_parallel_shared
{
  mulib_parallel_fn fn;
  void *context;
  const mulib_task_cancel_t *cancel;
  atomic_int error, cancelled;
} mulib_parallel_shared_t;
typedef struct mulib_parallel_chunk
{
  mulib_parallel_shared_t *shared;
  size_t begin, end;
} mulib_parallel_chunk_t;
static void
mulib_parallel_execute_ (void *arg)
{
  mulib_parallel_chunk_t *c = (mulib_parallel_chunk_t *)arg;
  size_t i;
  for (i = c->begin; i < c->end; ++i)
    {
      int rc, expected = 0;
      if (atomic_load (&c->shared->error)
          || atomic_load (&c->shared->cancelled))
        break;
      if (mulib_task_cancel_requested (c->shared->cancel))
        {
          atomic_store (&c->shared->cancelled, 1);
          break;
        }
      rc = c->shared->fn (i, c->shared->context);
      if (rc)
        {
          (void)atomic_compare_exchange_strong (&c->shared->error, &expected,
                                                rc);
          break;
        }
    }
}
static void
mulib_parallel_cancel_ (void *arg)
{
  mulib_parallel_chunk_t *c = (mulib_parallel_chunk_t *)arg;
  atomic_store (&c->shared->cancelled, 1);
}
int
mulib_parallel_for (mulib_taskpool_t *p, size_t begin, size_t end,
                    size_t grain, mulib_parallel_fn fn, void *arg,
                    const mulib_task_cancel_t *cancel, int *error)
{
  mulib_parallel_shared_t shared;
  mulib_parallel_chunk_t *chunks;
  size_t n, i, pos;
  int rc = MULIB_TASK_OK, code;
  if (error)
    *error = 0;
  if (!p || !fn || !grain || end < begin)
    return MULIB_TASK_INVALID;
  /* Reject same-pool reentry before submitting jobs that borrow this stack. */
  rc = mulib_taskpool_wait (p);
  if (rc)
    return rc;
  if (mulib_task_cancel_requested (cancel))
    return MULIB_TASK_CANCELLED;
  if (begin == end)
    return MULIB_TASK_OK;
  n = (end - begin) / grain + ((end - begin) % grain != 0);
  if (n > SIZE_MAX / sizeof (*chunks))
    return MULIB_TASK_INVALID;
  chunks = (mulib_parallel_chunk_t *)malloc (n * sizeof (*chunks));
  if (!chunks)
    return MULIB_TASK_NOMEM;
  shared.fn = fn;
  shared.context = arg;
  shared.cancel = cancel;
  atomic_init (&shared.error, 0);
  atomic_init (&shared.cancelled, 0);
  pos = begin;
  for (i = 0; i < n; ++i)
    {
      chunks[i].shared = &shared;
      chunks[i].begin = pos;
      chunks[i].end = end - pos < grain ? end : pos + grain;
      pos = chunks[i].end;
      if (atomic_load (&shared.error) || atomic_load (&shared.cancelled)
          || mulib_task_cancel_requested (cancel))
        break;
      do
        {
          rc = mulib_taskpool_submit (p, mulib_parallel_execute_, &chunks[i],
                                      mulib_parallel_cancel_);
          if (rc == MULIB_TASK_FULL)
            (void)mulib_taskpool_wait (p);
        }
      while (rc == MULIB_TASK_FULL);
      if (rc)
        break;
    }
  (void)mulib_taskpool_wait (p);
  code = atomic_load (&shared.error);
  if (error)
    *error = code;
  if (!rc && code)
    rc = MULIB_TASK_FAILED;
  if (!rc
      && (atomic_load (&shared.cancelled)
          || mulib_task_cancel_requested (cancel)))
    rc = MULIB_TASK_CANCELLED;
  free (chunks);
  return rc;
}
typedef struct mulib_parallel_map_context
{
  const unsigned char *input;
  unsigned char *output;
  size_t input_stride, output_stride;
  mulib_parallel_map_fn fn;
  void *context;
} mulib_parallel_map_context_t;
static int
mulib_parallel_map_one_ (size_t i, void *arg)
{
  mulib_parallel_map_context_t *c = (mulib_parallel_map_context_t *)arg;
  return c->fn (i, c->input + i * c->input_stride,
                c->output + i * c->output_stride, c->context);
}
int
mulib_parallel_map (mulib_taskpool_t *p, const void *in, size_t is, void *out,
                    size_t os, size_t count, size_t grain,
                    mulib_parallel_map_fn fn, void *arg,
                    const mulib_task_cancel_t *cancel, int *error)
{
  mulib_parallel_map_context_t c;
  size_t ib, ob;
  uintptr_t a = (uintptr_t)in, b = (uintptr_t)out;
  if (error)
    *error = 0;
  if (!fn || !is || !os || (count && (!in || !out)) || count > SIZE_MAX / is
      || count > SIZE_MAX / os)
    return MULIB_TASK_INVALID;
  ib = count * is;
  ob = count * os;
  if (count && !(in == out && is == os)
      && ((a <= b && b - a < ib) || (b < a && a - b < ob)))
    return MULIB_TASK_INVALID;
  c.input = (const unsigned char *)in;
  c.output = (unsigned char *)out;
  c.input_stride = is;
  c.output_stride = os;
  c.fn = fn;
  c.context = arg;
  return mulib_parallel_for (p, 0, count, grain, mulib_parallel_map_one_, &c,
                             cancel, error);
}
#endif
