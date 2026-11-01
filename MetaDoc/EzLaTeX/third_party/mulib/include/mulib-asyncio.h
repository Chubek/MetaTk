/**
 * @file mulib-asyncio.h
 * @brief Bounded single-threaded readiness reactor and FIFO callback jobs.
 * Define MULIB_IMPL_ASYNCIO and (separately) MULIB_IMPL_SELECT exactly once.
 * Borrows descriptors, including pipes, sockets, eventfd, timerfd and inotify.
 */
#ifndef MULIB_ASYNCIO_H
#define MULIB_ASYNCIO_H
#include "mulib-select.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_asyncio mulib_asyncio;
  typedef void (*mulib_asyncio_callback) (mulib_asyncio *loop, int fd,
                                          unsigned events, void *user);
  typedef void (*mulib_asyncio_job) (mulib_asyncio *loop, void *user);
  /** @brief Allocate fixed capacities: 1..select_limit watches, 1..65536 jobs.
   */
  mulib_asyncio *mulib_asyncio_create (size_t watch_capacity,
                                       size_t job_capacity);
  /** @brief Free idle loop (NULL allowed); EBUSY during callbacks; never
   * closes descriptors. */
  int mulib_asyncio_destroy (mulib_asyncio *loop);
  /** @brief Borrow fd until unwatch; unique fd, select interest flags; output
   * id on success. */
  int mulib_asyncio_watch (mulib_asyncio *loop, int fd, unsigned events,
                           mulib_asyncio_callback callback, void *user,
                           uint64_t *id);
  /** @brief Remove id; safe within callbacks and invalidates pending
   * readiness. */
  int mulib_asyncio_unwatch (mulib_asyncio *loop, uint64_t id);
  /** @brief Queue a borrowed user pointer and FIFO job; ENOSPC when full. */
  int mulib_asyncio_post (mulib_asyncio *loop, mulib_asyncio_job callback,
                          void *user);
  /** @brief Run entry-time jobs then one readiness batch with finite timeout;
   * callback count/-1. */
  int mulib_asyncio_run_once (mulib_asyncio *loop, int timeout_ms);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ASYNCIO) && !defined(MULIB_ASYNCIO_IMPLEMENTED)
#define MULIB_ASYNCIO_IMPLEMENTED
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifndef MULIB_ASYNCIO_ALLOC
#define MULIB_ASYNCIO_ALLOC(size) malloc (size)
#endif
#ifndef MULIB_ASYNCIO_FREE
#define MULIB_ASYNCIO_FREE(pointer) free (pointer)
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_asyncio_watch_
  {
    int fd;
    unsigned events;
    uint64_t id;
    mulib_asyncio_callback callback;
    void *user;
  } mulib_asyncio_watch_;
  typedef struct mulib_asyncio_job_
  {
    mulib_asyncio_job callback;
    void *user;
  } mulib_asyncio_job_;
  typedef struct mulib_asyncio_ref_
  {
    size_t slot;
    uint64_t id;
  } mulib_asyncio_ref_;
  struct mulib_asyncio
  {
    mulib_asyncio_watch_ *watches;
    mulib_select_entry *ready;
    mulib_asyncio_ref_ *references;
    mulib_asyncio_job_ *jobs;
    size_t watch_capacity, job_capacity, head, count;
    uint64_t next_id;
    int running;
  };
  mulib_asyncio *
  mulib_asyncio_create (size_t watch_capacity, size_t job_capacity)
  {
    mulib_asyncio *loop;
    int limit = mulib_select_limit ();
    if (limit < 0)
      return NULL;
    if (!watch_capacity || watch_capacity > (size_t)limit || !job_capacity
        || job_capacity > 65536 || job_capacity > (size_t)INT_MAX
        || watch_capacity > (size_t)INT_MAX - job_capacity
        || watch_capacity > SIZE_MAX / sizeof (mulib_asyncio_watch_)
        || watch_capacity > SIZE_MAX / sizeof (mulib_select_entry)
        || watch_capacity > SIZE_MAX / sizeof (mulib_asyncio_ref_)
        || job_capacity > SIZE_MAX / sizeof (mulib_asyncio_job_))
      {
        errno = EINVAL;
        return NULL;
      }
    loop = (mulib_asyncio *)MULIB_ASYNCIO_ALLOC (sizeof *loop);
    if (!loop)
      {
        errno = ENOMEM;
        return NULL;
      }
    /* Initialize pointers with C values, not all-bits-zero pointer
     * assumptions. */
    loop->watches = NULL;
    loop->ready = NULL;
    loop->references = NULL;
    loop->jobs = NULL;
    loop->watch_capacity = watch_capacity;
    loop->job_capacity = job_capacity;
    loop->head = 0;
    loop->count = 0;
    loop->next_id = 1;
    loop->running = 0;
    loop->watches = (mulib_asyncio_watch_ *)MULIB_ASYNCIO_ALLOC (
        watch_capacity * sizeof *loop->watches);
    loop->ready = (mulib_select_entry *)MULIB_ASYNCIO_ALLOC (
        watch_capacity * sizeof *loop->ready);
    loop->references = (mulib_asyncio_ref_ *)MULIB_ASYNCIO_ALLOC (
        watch_capacity * sizeof *loop->references);
    loop->jobs = (mulib_asyncio_job_ *)MULIB_ASYNCIO_ALLOC (
        job_capacity * sizeof *loop->jobs);
    if (!loop->watches || !loop->ready || !loop->references || !loop->jobs)
      {
        (void)mulib_asyncio_destroy (loop);
        errno = ENOMEM;
        return NULL;
      }
    {
      size_t i;
      for (i = 0; i < watch_capacity; ++i)
        loop->watches[i].id = 0;
    }
    return loop;
  }
  int
  mulib_asyncio_destroy (mulib_asyncio *loop)
  {
    if (!loop)
      return 0;
    if (loop->running)
      {
        errno = EBUSY;
        return -1;
      }
    MULIB_ASYNCIO_FREE (loop->watches);
    MULIB_ASYNCIO_FREE (loop->ready);
    MULIB_ASYNCIO_FREE (loop->references);
    MULIB_ASYNCIO_FREE (loop->jobs);
    MULIB_ASYNCIO_FREE (loop);
    return 0;
  }
  int
  mulib_asyncio_watch (mulib_asyncio *loop, int fd, unsigned events,
                       mulib_asyncio_callback callback, void *user,
                       uint64_t *id)
  {
    size_t i, free_slot;
    int limit = mulib_select_limit ();
    if (limit < 0)
      return -1;
    if (!loop || fd < 0 || fd >= limit || !events || (events & ~7u)
        || !callback || !id)
      {
        errno = EINVAL;
        return -1;
      }
    free_slot = loop->watch_capacity;
    for (i = 0; i < loop->watch_capacity; ++i)
      {
        if (loop->watches[i].id && loop->watches[i].fd == fd)
          {
            errno = EEXIST;
            return -1;
          }
        if (!loop->watches[i].id && free_slot == loop->watch_capacity)
          free_slot = i;
      }
    if (free_slot == loop->watch_capacity)
      {
        errno = ENOSPC;
        return -1;
      }
    if (!loop->next_id)
      {
        errno = EOVERFLOW;
        return -1;
      }
    loop->watches[free_slot].fd = fd;
    loop->watches[free_slot].events = events;
    loop->watches[free_slot].callback = callback;
    loop->watches[free_slot].user = user;
    loop->watches[free_slot].id = loop->next_id++;
    *id = loop->watches[free_slot].id;
    return 0;
  }
  int
  mulib_asyncio_unwatch (mulib_asyncio *loop, uint64_t id)
  {
    size_t i;
    if (!loop || !id)
      {
        errno = EINVAL;
        return -1;
      }
    for (i = 0; i < loop->watch_capacity; ++i)
      {
        if (loop->watches[i].id == id)
          {
            loop->watches[i].id = 0;
            return 0;
          }
      }
    errno = ENOENT;
    return -1;
  }
  int
  mulib_asyncio_post (mulib_asyncio *loop, mulib_asyncio_job callback,
                      void *user)
  {
    size_t index;
    if (!loop || !callback)
      {
        errno = EINVAL;
        return -1;
      }
    if (loop->count == loop->job_capacity)
      {
        errno = ENOSPC;
        return -1;
      }
    index = (loop->head + loop->count) % loop->job_capacity;
    loop->jobs[index].callback = callback;
    loop->jobs[index].user = user;
    ++loop->count;
    return 0;
  }
  int
  mulib_asyncio_run_once (mulib_asyncio *loop, int timeout_ms)
  {
    size_t initial_jobs, i, count = 0;
    int invoked = 0, result;
    if (!loop || timeout_ms < 0)
      {
        errno = EINVAL;
        return -1;
      }
    if (loop->running)
      {
        errno = EBUSY;
        return -1;
      }
    loop->running = 1;
    initial_jobs = loop->count;
    for (i = 0; i < initial_jobs; ++i)
      {
        mulib_asyncio_job_ job = loop->jobs[loop->head];
        loop->head = (loop->head + 1) % loop->job_capacity;
        --loop->count;
        job.callback (loop, job.user);
        ++invoked;
      }
    for (i = 0; i < loop->watch_capacity; ++i)
      {
        if (loop->watches[i].id)
          {
            loop->ready[count].fd = loop->watches[i].fd;
            loop->ready[count].events = loop->watches[i].events;
            loop->references[count].slot = i;
            loop->references[count].id = loop->watches[i].id;
            ++count;
          }
      }
    result = mulib_select_wait (loop->ready, count,
                                initial_jobs ? 0 : timeout_ms);
    if (result < 0)
      {
        loop->running = 0;
        return -1;
      }
    for (i = 0; i < count; ++i)
      {
        mulib_asyncio_watch_ *watch = &loop->watches[loop->references[i].slot];
        if (loop->ready[i].revents && watch->id == loop->references[i].id)
          {
            mulib_asyncio_callback callback = watch->callback;
            int fd = watch->fd;
            void *user = watch->user;
            callback (loop, fd, loop->ready[i].revents, user);
            ++invoked;
          }
      }
    loop->running = 0;
    return invoked;
  }
#ifdef __cplusplus
}
#endif
#endif
