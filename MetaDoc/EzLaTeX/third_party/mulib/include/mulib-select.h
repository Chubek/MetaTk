/**
 * @file mulib-select.h
 * @brief Bounded POSIX select readiness; no descriptor ownership or
 * allocation. Define MULIB_IMPL_SELECT in exactly one translation unit. See
 * the manual.
 */
#ifndef MULIB_SELECT_H
#define MULIB_SELECT_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_SELECT_READ = 1,
    MULIB_SELECT_WRITE = 2,
    MULIB_SELECT_EXCEPT = 4
  };
  typedef struct mulib_select_entry
  {
    int fd;
    unsigned events, revents;
  } mulib_select_entry;
  /** @brief Return the exclusive descriptor/count limit, or -1 with ENOSYS. */
  int mulib_select_limit (void);
  /** @brief Wait once; return ready entry count, or -1 with errno. Timeout -1
   * waits indefinitely. */
  int mulib_select_wait (mulib_select_entry *entries, size_t count,
                         int timeout_ms);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SELECT) && !defined(MULIB_SELECT_IMPLEMENTED)
#define MULIB_SELECT_IMPLEMENTED
#include <errno.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/select.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_select_limit (void)
  {
#if defined(__unix__) || defined(__APPLE__)
    return FD_SETSIZE;
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_select_wait (mulib_select_entry *entries, size_t count, int timeout_ms)
  {
    size_t i;
    if ((!entries && count) || timeout_ms < -1)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__unix__) || defined(__APPLE__)
    fd_set reads, writes, excepts;
    struct timeval timeout;
    int maxfd = -1, result, ready = 0;
    if (count > (size_t)FD_SETSIZE)
      {
        errno = EINVAL;
        return -1;
      }
    for (i = 0; i < count; ++i)
      {
        if (entries[i].fd < 0 || entries[i].fd >= FD_SETSIZE
            || !entries[i].events || (entries[i].events & ~7u))
          {
            errno = EINVAL;
            return -1;
          }
      }
    FD_ZERO (&reads);
    FD_ZERO (&writes);
    FD_ZERO (&excepts);
    for (i = 0; i < count; ++i)
      {
        int fd = entries[i].fd;
        entries[i].revents = 0;
        if (entries[i].events & MULIB_SELECT_READ)
          FD_SET (fd, &reads);
        if (entries[i].events & MULIB_SELECT_WRITE)
          FD_SET (fd, &writes);
        if (entries[i].events & MULIB_SELECT_EXCEPT)
          FD_SET (fd, &excepts);
        if (fd > maxfd)
          maxfd = fd;
      }
    timeout.tv_sec = timeout_ms < 0 ? 0 : timeout_ms / 1000;
    timeout.tv_usec = timeout_ms < 0 ? 0 : (timeout_ms % 1000) * 1000;
    result = select (maxfd + 1, &reads, &writes, &excepts,
                     timeout_ms < 0 ? NULL : &timeout);
    if (result < 0)
      return -1;
    for (i = 0; i < count; ++i)
      {
        int fd = entries[i].fd;
        if (FD_ISSET (fd, &reads))
          entries[i].revents |= MULIB_SELECT_READ;
        if (FD_ISSET (fd, &writes))
          entries[i].revents |= MULIB_SELECT_WRITE;
        if (FD_ISSET (fd, &excepts))
          entries[i].revents |= MULIB_SELECT_EXCEPT;
        if (entries[i].revents)
          ++ready;
      }
    return ready;
#else
  (void)i;
  errno = ENOSYS;
  return -1;
#endif
  }
#ifdef __cplusplus
}
#endif
#endif
