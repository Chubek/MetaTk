/**
 * @file mulib-epoll.h
 * @brief Native Linux epoll with opaque application tokens and bounded
 * batches. Define MULIB_IMPL_EPOLL once. Other platforms fail with ENOSYS.
 */
#ifndef MULIB_EPOLL_H
#define MULIB_EPOLL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_EPOLL_READ = 1,
    MULIB_EPOLL_WRITE = 2,
    MULIB_EPOLL_PRIORITY = 4,
    MULIB_EPOLL_ERROR = 8,
    MULIB_EPOLL_HANGUP = 16,
    MULIB_EPOLL_RDHUP = 32,
    MULIB_EPOLL_EDGE = 64,
    MULIB_EPOLL_ONESHOT = 128,
    MULIB_EPOLL_BATCH_MAX = 256
  };
  typedef struct mulib_epoll_event
  {
    unsigned events;
    uint64_t token;
  } mulib_epoll_event;
  /** @brief Create an owned close-on-exec epoll descriptor, or -1 with errno.
   */
  int mulib_epoll_create (void);
  /** @brief Register a borrowed descriptor and application token; return 0/-1.
   */
  int mulib_epoll_add (int epfd, int fd, unsigned events, uint64_t token);
  /** @brief Replace a registration (also rearms ONESHOT); return 0/-1. */
  int mulib_epoll_modify (int epfd, int fd, unsigned events, uint64_t token);
  /** @brief Remove a registration, never closing the watched descriptor. */
  int mulib_epoll_remove (int epfd, int fd);
  /** @brief Wait once for 1..256 events; return count or -1, propagating
   * EINTR. */
  int mulib_epoll_wait (int epfd, mulib_epoll_event *events, size_t capacity,
                        int timeout_ms);
  /** @brief Close once; never retry a failed close on the same descriptor
   * number. */
  int mulib_epoll_close (int epfd);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EPOLL) && !defined(MULIB_EPOLL_IMPLEMENTED)
#define MULIB_EPOLL_IMPLEMENTED
#include <errno.h>
#include <string.h>
#if defined(__linux__)
#include <sys/epoll.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_epoll_create (void)
  {
#if defined(__linux__)
    return epoll_create1 (EPOLL_CLOEXEC);
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  static int
  mulib_epoll_control_ (int epfd, int fd, unsigned events, uint64_t token,
                        int modify)
  {
    const unsigned allowed = MULIB_EPOLL_READ | MULIB_EPOLL_WRITE
                             | MULIB_EPOLL_PRIORITY | MULIB_EPOLL_RDHUP
                             | MULIB_EPOLL_EDGE | MULIB_EPOLL_ONESHOT;
    if (epfd < 0 || fd < 0 || (events & ~allowed))
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
    struct epoll_event native_event;
    memset (&native_event, 0, sizeof native_event);
    if (events & MULIB_EPOLL_READ)
      native_event.events |= EPOLLIN;
    if (events & MULIB_EPOLL_WRITE)
      native_event.events |= EPOLLOUT;
    if (events & MULIB_EPOLL_PRIORITY)
      native_event.events |= EPOLLPRI;
    if (events & MULIB_EPOLL_RDHUP)
      native_event.events |= EPOLLRDHUP;
    if (events & MULIB_EPOLL_EDGE)
      native_event.events |= EPOLLET;
    if (events & MULIB_EPOLL_ONESHOT)
      native_event.events |= EPOLLONESHOT;
    native_event.data.u64 = token;
    return epoll_ctl (epfd, modify ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, fd,
                      &native_event);
#else
  (void)token;
  (void)modify;
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_epoll_add (int epfd, int fd, unsigned events, uint64_t token)
  {
    return mulib_epoll_control_ (epfd, fd, events, token, 0);
  }
  int
  mulib_epoll_modify (int epfd, int fd, unsigned events, uint64_t token)
  {
    return mulib_epoll_control_ (epfd, fd, events, token, 1);
  }
  int
  mulib_epoll_remove (int epfd, int fd)
  {
    if (epfd < 0 || fd < 0)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
    return epoll_ctl (epfd, EPOLL_CTL_DEL, fd, NULL);
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_epoll_wait (int epfd, mulib_epoll_event *events, size_t capacity,
                    int timeout_ms)
  {
    if (epfd < 0 || !events || !capacity || capacity > MULIB_EPOLL_BATCH_MAX
        || timeout_ms < -1)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
    struct epoll_event native_events[MULIB_EPOLL_BATCH_MAX];
    int count = epoll_wait (epfd, native_events, (int)capacity, timeout_ms), i;
    if (count < 0)
      return -1;
    for (i = 0; i < count; ++i)
      {
        uint32_t flags = native_events[i].events;
        unsigned out = 0;
        if (flags & EPOLLIN)
          out |= MULIB_EPOLL_READ;
        if (flags & EPOLLOUT)
          out |= MULIB_EPOLL_WRITE;
        if (flags & EPOLLPRI)
          out |= MULIB_EPOLL_PRIORITY;
        if (flags & EPOLLERR)
          out |= MULIB_EPOLL_ERROR;
        if (flags & EPOLLHUP)
          out |= MULIB_EPOLL_HANGUP;
        if (flags & EPOLLRDHUP)
          out |= MULIB_EPOLL_RDHUP;
        events[i].events = out;
        events[i].token = native_events[i].data.u64;
      }
    return count;
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_epoll_close (int epfd)
  {
    if (epfd < 0)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
    return close (epfd);
#else
  errno = ENOSYS;
  return -1;
#endif
  }
#ifdef __cplusplus
}
#endif
#endif
