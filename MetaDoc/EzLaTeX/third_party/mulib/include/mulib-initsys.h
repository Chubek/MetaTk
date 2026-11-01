/**
 * @file mulib-initsys.h
 * @brief Dependency-ordered service lifecycle and foreground child
 * supervision. Supply MULIB_IMPL_INITSYS, TASKDAG, TASKPOOL and PROCESS
 * separately. This is an embeddable supervisor, not a system init/PID-1
 * implementation.
 */
#ifndef MULIB_INITSYS_H
#define MULIB_INITSYS_H
#include "mulib-process.h"
#include "mulib-taskdag.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_initsys mulib_initsys_t;
  typedef int (*mulib_initsys_fn) (void *context);
  /** Allocate fixed service/dependency capacities. */
  int mulib_initsys_create (mulib_initsys_t **out, size_t services,
                            size_t dependencies);
  /** Borrow callbacks and context. Successful start is paired with stop,
   * including rollback. A failed start must clean up its own partial
   * resources. */
  int mulib_initsys_add (mulib_initsys_t *system, mulib_initsys_fn start,
                         mulib_initsys_fn stop, void *context, size_t *id);
  /** Add a foreground child service. file/argv/strings are borrowed until
   * destroy. Start means spawn succeeded, not readiness. Stop forcibly kills
   * and reaps. */
  int mulib_initsys_add_process (mulib_initsys_t *system, const char *file,
                                 const char *const *argv, size_t argc,
                                 const mulib_process_io_t *io, size_t *id);
  /** Add prerequisite -> dependent while stopped. */
  int mulib_initsys_depend (mulib_initsys_t *system, size_t prerequisite,
                            size_t dependent);
  /** Start using taskdag; on any failure/cancellation stop all successful
   * starts. May only be called externally to the supplied, exclusively
   * controlled pool. */
  int mulib_initsys_start (mulib_initsys_t *system, mulib_taskpool_t *pool,
                           const mulib_task_cancel_t *cancel);
  /** Stop in reverse topological order. Continue after errors; failed stops
   * stay live for a later retry. Custom stop callbacks run on the calling
   * thread. */
  int mulib_initsys_stop (mulib_initsys_t *system);
  /** Nonblocking child check. If a managed child exited (even zero), reap it
   * and stop all remaining services; return TASK_FAILED. No automatic restart.
   * exited/status are optional; service IDs identify the unexpected exit. */
  int mulib_initsys_check (mulib_initsys_t *system, size_t *exited,
                           mulib_process_status_t *status);
  /** Query whether a service needs stopping. Serialized lifecycle access only.
   */
  int mulib_initsys_live (const mulib_initsys_t *system, size_t id);
  /** Stop then free on success; on stop failure retain the system for retry.
   * Returns status, unlike an unconditional free that could orphan children.
   */
  int mulib_initsys_destroy (mulib_initsys_t *system);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_INITSYS) && !defined(MULIB_INITSYS_IMPLEMENTED)
#define MULIB_INITSYS_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
typedef struct mulib_initsys_service
{
  mulib_initsys_fn start, stop;
  void *context;
  const char *file;
  const char *const *argv;
  size_t argc;
  mulib_process_io_t io;
  mulib_process_t process;
  int process_service, live;
} mulib_initsys_service_t;
struct mulib_initsys
{
  mulib_taskdag_t *dag;
  mulib_initsys_service_t *services;
  size_t *order;
  size_t count, capacity;
};
static int
mulib_initsys_start_one_ (void *arg)
{
  mulib_initsys_service_t *s = (mulib_initsys_service_t *)arg;
  int rc = s->process_service ? mulib_process_spawn (&s->process, s->file,
                                                     s->argv, s->argc, &s->io)
                              : s->start (s->context);
  if (!rc)
    s->live = 1;
  return rc;
}
static int
mulib_initsys_any_live_ (const mulib_initsys_t *s)
{
  size_t i;
  for (i = 0; i < s->count; ++i)
    if (s->services[i].live)
      return 1;
  return 0;
}
int
mulib_initsys_create (mulib_initsys_t **out, size_t cap, size_t edges)
{
  mulib_initsys_t *s;
  int rc;
  if (!out || !cap || cap > SIZE_MAX / sizeof (mulib_initsys_service_t)
      || cap > SIZE_MAX / sizeof (size_t))
    return MULIB_TASK_INVALID;
  *out = NULL;
  s = (mulib_initsys_t *)calloc (1, sizeof (*s));
  if (!s)
    return MULIB_TASK_NOMEM;
  s->services = (mulib_initsys_service_t *)calloc (cap, sizeof (*s->services));
  s->order = (size_t *)malloc (cap * sizeof (*s->order));
  if (!s->services || !s->order)
    {
      free (s->services);
      free (s->order);
      free (s);
      return MULIB_TASK_NOMEM;
    }
  rc = mulib_taskdag_create (&s->dag, cap, edges);
  if (rc)
    {
      free (s->services);
      free (s->order);
      free (s);
      return rc;
    }
  s->capacity = cap;
  *out = s;
  return MULIB_TASK_OK;
}
int
mulib_initsys_add (mulib_initsys_t *s, mulib_initsys_fn start,
                   mulib_initsys_fn stop, void *arg, size_t *id)
{
  int rc;
  mulib_initsys_service_t *service;
  if (!s || !start || !stop || !id || mulib_initsys_any_live_ (s))
    return MULIB_TASK_INVALID;
  if (s->count == s->capacity)
    return MULIB_TASK_FULL;
  service = &s->services[s->count];
  rc = mulib_taskdag_add (s->dag, mulib_initsys_start_one_, service, id);
  if (rc)
    return rc;
  service->start = start;
  service->stop = stop;
  service->context = arg;
  ++s->count;
  return MULIB_TASK_OK;
}
int
mulib_initsys_add_process (mulib_initsys_t *s, const char *file,
                           const char *const *argv, size_t argc,
                           const mulib_process_io_t *io, size_t *id)
{
  mulib_initsys_service_t *service;
  size_t i;
  int rc;
  if (!s || !file || !*file || !argv || !argc || !id
      || argc > SIZE_MAX / sizeof (char *) - 1 || mulib_initsys_any_live_ (s))
    return MULIB_TASK_INVALID;
  for (i = 0; i < argc; ++i)
    if (!argv[i])
      return MULIB_TASK_INVALID;
  if (io
      && (io->stdin_fd < -1 || io->stdout_fd < -1 || io->stderr_fd < -1
          || (io->stdin_fd >= 0 && io->stdin_fd < 3 && io->stdin_fd != 0)
          || (io->stdout_fd >= 0 && io->stdout_fd < 3 && io->stdout_fd != 1)
          || (io->stderr_fd >= 0 && io->stderr_fd < 3 && io->stderr_fd != 2)))
    return MULIB_TASK_INVALID;
  if (s->count == s->capacity)
    return MULIB_TASK_FULL;
  service = &s->services[s->count];
  rc = mulib_taskdag_add (s->dag, mulib_initsys_start_one_, service, id);
  if (rc)
    return rc;
  service->process_service = 1;
  service->file = file;
  service->argv = argv;
  service->argc = argc;
  service->io.stdin_fd = service->io.stdout_fd = service->io.stderr_fd = -1;
  if (io)
    service->io = *io;
  mulib_process_init (&service->process);
  ++s->count;
  return MULIB_TASK_OK;
}
int
mulib_initsys_depend (mulib_initsys_t *s, size_t a, size_t b)
{
  if (!s || mulib_initsys_any_live_ (s))
    return MULIB_TASK_INVALID;
  return mulib_taskdag_depend (s->dag, a, b);
}
int
mulib_initsys_start (mulib_initsys_t *s, mulib_taskpool_t *p,
                     const mulib_task_cancel_t *cancel)
{
  int rc, stopped;
  if (!s || !p || mulib_initsys_any_live_ (s))
    return MULIB_TASK_INVALID;
  rc = mulib_taskdag_run (s->dag, p, cancel);
  if (!rc)
    return MULIB_TASK_OK;
  stopped = mulib_initsys_stop (s);
  return stopped ? stopped : rc;
}
int
mulib_initsys_stop (mulib_initsys_t *s)
{
  size_t i;
  int rc, result = MULIB_TASK_OK;
  if (!s)
    return MULIB_TASK_INVALID;
  if (!mulib_initsys_any_live_ (s))
    return MULIB_TASK_OK;
  rc = mulib_taskdag_order (s->dag, s->order, s->capacity);
  if (rc)
    return rc;
  for (i = s->count; i > 0; --i)
    {
      mulib_initsys_service_t *service = &s->services[s->order[i - 1]];
      mulib_process_status_t status;
      if (!service->live)
        continue;
      rc = service->process_service
               ? mulib_process_terminate (&service->process, &status)
               : service->stop (service->context);
      if (!rc || (service->process_service && !service->process.pid))
        service->live = 0;
      if (rc)
        result = MULIB_TASK_FAILED;
    }
  return result;
}
int
mulib_initsys_check (mulib_initsys_t *s, size_t *exited,
                     mulib_process_status_t *status)
{
  size_t i;
  if (!s)
    return MULIB_TASK_INVALID;
  for (i = 0; i < s->count; ++i)
    if (s->services[i].process_service && s->services[i].live)
      {
        mulib_process_status_t observed;
        int rc = mulib_process_wait (&s->services[i].process, &observed, 1);
        if (rc == MULIB_PROCESS_RUNNING)
          continue;
        if (rc)
          {
            if (!s->services[i].process.pid)
              {
                s->services[i].live = 0;
                (void)mulib_initsys_stop (s);
              }
            return MULIB_TASK_SYSTEM;
          }
        s->services[i].live = 0;
        if (exited)
          *exited = i;
        if (status)
          *status = observed;
        (void)mulib_initsys_stop (s);
        return MULIB_TASK_FAILED;
      }
  return MULIB_TASK_OK;
}
int
mulib_initsys_live (const mulib_initsys_t *s, size_t id)
{
  if (!s || id >= s->count)
    return MULIB_TASK_INVALID;
  return s->services[id].live;
}
int
mulib_initsys_destroy (mulib_initsys_t *s)
{
  int rc;
  if (!s)
    return MULIB_TASK_OK;
  rc = mulib_initsys_stop (s);
  if (rc)
    return rc;
  mulib_taskdag_destroy (s->dag);
  free (s->services);
  free (s->order);
  free (s);
  return MULIB_TASK_OK;
}
#endif
