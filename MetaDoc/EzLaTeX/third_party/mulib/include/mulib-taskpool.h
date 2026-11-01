/**
 * @file mulib-taskpool.h
 * @brief Bounded pthread worker queue and cooperative cancellation tokens.
 * Define MULIB_IMPL_TASKPOOL once; link with -pthread on supported POSIX
 * hosts. See manual/mulib-taskpool.md for ownership and lifecycle
 * restrictions.
 */
#ifndef MULIB_TASKPOOL_H
#define MULIB_TASKPOOL_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_taskpool mulib_taskpool_t;
  typedef struct mulib_task_cancel mulib_task_cancel_t;
  typedef void (*mulib_task_fn) (void *context);
  enum
  {
    MULIB_TASK_OK = 0,
    MULIB_TASK_INVALID = -1,
    MULIB_TASK_NOMEM = -2,
    MULIB_TASK_UNSUPPORTED = -3,
    MULIB_TASK_FULL = -4,
    MULIB_TASK_CLOSED = -5,
    MULIB_TASK_CANCELLED = -6,
    MULIB_TASK_CYCLE = -7,
    MULIB_TASK_FAILED = -8,
    MULIB_TASK_SYSTEM = -9
  };
  /** Allocate workers and a queue of exactly capacity pending tasks. */
  int mulib_taskpool_create (mulib_taskpool_t **out, size_t workers,
                             size_t capacity);
  /** Nonblocking submission. On success only, ownership of context transfers
   * to fn or on_cancel (exactly one). NULL on_cancel means no pending-task
   * cleanup. */
  int mulib_taskpool_submit (mulib_taskpool_t *pool, mulib_task_fn fn,
                             void *context, mulib_task_fn on_cancel);
  /** Wait for global pool idleness. Same-pool callbacks return TASK_INVALID.
   */
  int mulib_taskpool_wait (mulib_taskpool_t *pool);
  /** Reject new work; optionally discard pending tasks, invoking their
   * on_cancel. Running callbacks finish normally. Lifecycle calls require one
   * controller; same-pool callbacks return TASK_INVALID. */
  int mulib_taskpool_shutdown (mulib_taskpool_t *pool, int cancel_pending);
  /** Shutdown, join every worker and free. NULL is harmless. No concurrent
   * callers. Calls from same-pool callbacks are ignored, retaining ownership
   * for the controller. */
  void mulib_taskpool_destroy (mulib_taskpool_t *pool, int cancel_pending);
  /** Allocate a thread-safe, initially clear cooperative cancellation token.
   */
  mulib_task_cancel_t *mulib_task_cancel_create (void);
  /** Request cancellation; NULL is harmless. There is deliberately no reset.
   */
  void mulib_task_cancel_request (mulib_task_cancel_t *token);
  /** NULL means cancellation was not requested. */
  int mulib_task_cancel_requested (const mulib_task_cancel_t *token);
  /** Free after all users have finished; NULL is harmless. */
  void mulib_task_cancel_destroy (mulib_task_cancel_t *token);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_TASKPOOL) && !defined(MULIB_TASKPOOL_IMPLEMENTED)
#define MULIB_TASKPOOL_IMPLEMENTED
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#if !defined(MULIB_TASKPOOL_NO_PTHREAD)                                       \
    && (defined(__unix__) || defined(__APPLE__))
#define MULIB_TASKPOOL_HAS_PTHREAD 1
#include <pthread.h>
#else
#define MULIB_TASKPOOL_HAS_PTHREAD 0
#endif
struct mulib_task_cancel
{
  atomic_int requested;
};
mulib_task_cancel_t *
mulib_task_cancel_create (void)
{
  mulib_task_cancel_t *t = (mulib_task_cancel_t *)malloc (sizeof (*t));
  if (t)
    atomic_init (&t->requested, 0);
  return t;
}
void
mulib_task_cancel_request (mulib_task_cancel_t *t)
{
  if (t)
    atomic_store (&t->requested, 1);
}
int
mulib_task_cancel_requested (const mulib_task_cancel_t *t)
{
  return t ? atomic_load (&t->requested) : 0;
}
void
mulib_task_cancel_destroy (mulib_task_cancel_t *t)
{
  free (t);
}
#if MULIB_TASKPOOL_HAS_PTHREAD
typedef struct mulib_taskpool_job
{
  mulib_task_fn fn, cancel;
  void *context;
} mulib_taskpool_job_t;
struct mulib_taskpool
{
  pthread_mutex_t mutex;
  pthread_cond_t ready, idle;
  pthread_t *threads;
  mulib_taskpool_job_t *queue;
  size_t workers, capacity, head, count, active;
  int closed, cancelling;
};
typedef struct mulib_taskpool_frame_
{
  mulib_taskpool_t *pool;
  struct mulib_taskpool_frame_ *previous;
} mulib_taskpool_frame_;
static _Thread_local mulib_taskpool_frame_ *mulib_taskpool_current_;
static int
mulib_taskpool_in_callback_ (mulib_taskpool_t *p)
{
  mulib_taskpool_frame_ *frame;
  for (frame = mulib_taskpool_current_; frame; frame = frame->previous)
    if (frame->pool == p)
      return 1;
  return 0;
}
static void
mulib_taskpool_call_ (mulib_taskpool_t *p, mulib_task_fn fn, void *arg)
{
  mulib_taskpool_frame_ frame;
  frame.pool = p;
  frame.previous = mulib_taskpool_current_;
  mulib_taskpool_current_ = &frame;
  fn (arg);
  mulib_taskpool_current_ = frame.previous;
}
static void *
mulib_taskpool_worker_ (void *arg)
{
  mulib_taskpool_t *p = (mulib_taskpool_t *)arg;
  (void)pthread_mutex_lock (&p->mutex);
  for (;;)
    {
      mulib_taskpool_job_t job;
      while (!p->count && !p->closed)
        (void)pthread_cond_wait (&p->ready, &p->mutex);
      if (p->cancelling || (!p->count && p->closed))
        break;
      job = p->queue[p->head];
      p->head = (p->head + 1) % p->capacity;
      --p->count;
      ++p->active;
      (void)pthread_mutex_unlock (&p->mutex);
      mulib_taskpool_call_ (p, job.fn, job.context);
      (void)pthread_mutex_lock (&p->mutex);
      --p->active;
      if (!p->count && !p->active)
        (void)pthread_cond_broadcast (&p->idle);
    }
  (void)pthread_mutex_unlock (&p->mutex);
  return NULL;
}
int
mulib_taskpool_create (mulib_taskpool_t **out, size_t workers, size_t capacity)
{
  mulib_taskpool_t *p;
  size_t i;
  if (!out || !workers || !capacity)
    return MULIB_TASK_INVALID;
  *out = NULL;
  if (workers > SIZE_MAX / sizeof (pthread_t)
      || capacity > SIZE_MAX / sizeof (mulib_taskpool_job_t))
    return MULIB_TASK_INVALID;
  p = (mulib_taskpool_t *)calloc (1, sizeof (*p));
  if (!p)
    return MULIB_TASK_NOMEM;
  p->threads = (pthread_t *)malloc (workers * sizeof (*p->threads));
  p->queue = (mulib_taskpool_job_t *)malloc (capacity * sizeof (*p->queue));
  if (!p->threads || !p->queue)
    {
      free (p->threads);
      free (p->queue);
      free (p);
      return MULIB_TASK_NOMEM;
    }
  if (pthread_mutex_init (&p->mutex, NULL))
    goto fail_alloc;
  if (pthread_cond_init (&p->ready, NULL))
    goto fail_mutex;
  if (pthread_cond_init (&p->idle, NULL))
    goto fail_ready;
  p->capacity = capacity;
  for (i = 0; i < workers; ++i)
    {
      if (pthread_create (&p->threads[i], NULL, mulib_taskpool_worker_, p))
        {
          p->workers = i;
          mulib_taskpool_destroy (p, 1);
          return MULIB_TASK_SYSTEM;
        }
    }
  p->workers = workers;
  *out = p;
  return MULIB_TASK_OK;
fail_ready:
  (void)pthread_cond_destroy (&p->ready);
fail_mutex:
  (void)pthread_mutex_destroy (&p->mutex);
fail_alloc:
  free (p->threads);
  free (p->queue);
  free (p);
  return MULIB_TASK_SYSTEM;
}
int
mulib_taskpool_submit (mulib_taskpool_t *p, mulib_task_fn fn, void *arg,
                       mulib_task_fn cancel)
{
  size_t tail;
  if (!p || !fn)
    return MULIB_TASK_INVALID;
  (void)pthread_mutex_lock (&p->mutex);
  if (p->closed || p->count == p->capacity)
    {
      int rc = p->closed ? MULIB_TASK_CLOSED : MULIB_TASK_FULL;
      (void)pthread_mutex_unlock (&p->mutex);
      return rc;
    }
  /* Avoid overflowing head + count. */
  tail = p->count >= p->capacity - p->head ? p->count - (p->capacity - p->head)
                                           : p->head + p->count;
  p->queue[tail].fn = fn;
  p->queue[tail].context = arg;
  p->queue[tail].cancel = cancel;
  ++p->count;
  (void)pthread_cond_signal (&p->ready);
  (void)pthread_mutex_unlock (&p->mutex);
  return MULIB_TASK_OK;
}
int
mulib_taskpool_wait (mulib_taskpool_t *p)
{
  if (!p || mulib_taskpool_in_callback_ (p))
    return MULIB_TASK_INVALID;
  (void)pthread_mutex_lock (&p->mutex);
  while (p->count || p->active)
    (void)pthread_cond_wait (&p->idle, &p->mutex);
  (void)pthread_mutex_unlock (&p->mutex);
  return MULIB_TASK_OK;
}
int
mulib_taskpool_shutdown (mulib_taskpool_t *p, int cancel_pending)
{
  if (!p || mulib_taskpool_in_callback_ (p))
    return MULIB_TASK_INVALID;
  (void)pthread_mutex_lock (&p->mutex);
  p->closed = 1;
  if (cancel_pending)
    p->cancelling = 1;
  (void)pthread_cond_broadcast (&p->ready);
  while (p->cancelling && p->count)
    {
      mulib_taskpool_job_t job = p->queue[p->head];
      p->head = (p->head + 1) % p->capacity;
      --p->count;
      ++p->active; /* wait also covers cancellation cleanup. */
      (void)pthread_mutex_unlock (&p->mutex);
      if (job.cancel)
        mulib_taskpool_call_ (p, job.cancel, job.context);
      (void)pthread_mutex_lock (&p->mutex);
      --p->active;
    }
  if (!p->count && !p->active)
    (void)pthread_cond_broadcast (&p->idle);
  (void)pthread_mutex_unlock (&p->mutex);
  return MULIB_TASK_OK;
}
void
mulib_taskpool_destroy (mulib_taskpool_t *p, int cancel_pending)
{
  size_t i;
  if (!p || mulib_taskpool_in_callback_ (p))
    return;
  (void)mulib_taskpool_shutdown (p, cancel_pending);
  for (i = 0; i < p->workers; ++i)
    (void)pthread_join (p->threads[i], NULL);
  (void)pthread_cond_destroy (&p->idle);
  (void)pthread_cond_destroy (&p->ready);
  (void)pthread_mutex_destroy (&p->mutex);
  free (p->threads);
  free (p->queue);
  free (p);
}
#else
int
mulib_taskpool_create (mulib_taskpool_t **out, size_t n, size_t cap)
{
  if (!out || !n || !cap)
    return MULIB_TASK_INVALID;
  *out = NULL;
  return MULIB_TASK_UNSUPPORTED;
}
int
mulib_taskpool_submit (mulib_taskpool_t *p, mulib_task_fn f, void *a,
                       mulib_task_fn c)
{
  (void)p;
  (void)f;
  (void)a;
  (void)c;
  return MULIB_TASK_UNSUPPORTED;
}
int
mulib_taskpool_wait (mulib_taskpool_t *p)
{
  (void)p;
  return MULIB_TASK_UNSUPPORTED;
}
int
mulib_taskpool_shutdown (mulib_taskpool_t *p, int c)
{
  (void)p;
  (void)c;
  return MULIB_TASK_UNSUPPORTED;
}
void
mulib_taskpool_destroy (mulib_taskpool_t *p, int c)
{
  (void)p;
  (void)c;
}
#endif
#endif
