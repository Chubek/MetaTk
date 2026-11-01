/**
 * @file mulib-syscall.h
 * @brief Focused POSIX descriptor operations, not an arbitrary syscall
 * dispatcher. Define MULIB_IMPL_SYSCALL once. Other platforms return ENOSYS.
 */
#ifndef MULIB_SYSCALL_H
#define MULIB_SYSCALL_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Create two owned nonblocking, close-on-exec pipe ends; output
   * atomic on failure. */
  int mulib_syscall_pipe (int descriptors[2]);
  /** @brief Change only O_NONBLOCK; enabled must be 0 or 1. */
  int mulib_syscall_nonblock (int fd, int enabled);
  /** @brief Perform one read, preserving output count on failure; zero count
   * means EOF. */
  int mulib_syscall_read (int fd, void *buffer, size_t size, size_t *count);
  /** @brief Perform one write, possibly short; caller must handle SIGPIPE
   * policy. */
  int mulib_syscall_write (int fd, const void *buffer, size_t size,
                           size_t *count);
  /** @brief Close once, never retry on EINTR; ownership is relinquished on
   * call. */
  int mulib_syscall_close (int fd);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYSCALL) && !defined(MULIB_SYSCALL_IMPLEMENTED)
#define MULIB_SYSCALL_IMPLEMENTED
#include <errno.h>
#include <stdint.h>
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
#if defined(__linux__) && defined(O_CLOEXEC)
  /* GNU feature macros may hide the declaration, but not the Linux libc ABI.
   */
  extern int pipe2 (int[2], int);
#endif
  int
  mulib_syscall_nonblock (int fd, int enabled)
  {
    if (fd < 0 || (enabled != 0 && enabled != 1))
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__unix__) || defined(__APPLE__)
    int flags = fcntl (fd, F_GETFL);
    if (flags < 0)
      return -1;
    return fcntl (fd, F_SETFL,
                  enabled ? flags | O_NONBLOCK : flags & ~O_NONBLOCK);
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_syscall_pipe (int descriptors[2])
  {
    if (!descriptors)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__unix__) || defined(__APPLE__)
    int pair[2];
#if defined(__linux__) && defined(O_CLOEXEC)
    if (pipe2 (pair, O_CLOEXEC | O_NONBLOCK) < 0)
      return -1;
    descriptors[0] = pair[0];
    descriptors[1] = pair[1];
    return 0;
#else
    int i, saved;
    if (pipe (pair) < 0)
      return -1;
    for (i = 0; i < 2; ++i)
      {
        int flags = fcntl (pair[i], F_GETFD);
        if (flags < 0 || fcntl (pair[i], F_SETFD, flags | FD_CLOEXEC) < 0
            || mulib_syscall_nonblock (pair[i], 1) < 0)
          {
            saved = errno;
            (void)close (pair[0]);
            (void)close (pair[1]);
            errno = saved;
            return -1;
          }
      }
    descriptors[0] = pair[0];
    descriptors[1] = pair[1];
    return 0;
#endif
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_syscall_read (int fd, void *buffer, size_t size, size_t *count)
  {
    if (fd < 0 || (!buffer && size) || !count || size > (size_t)PTRDIFF_MAX)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__unix__) || defined(__APPLE__)
    ssize_t n = read (fd, buffer, size);
    if (n < 0)
      return -1;
    *count = (size_t)n;
    return 0;
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_syscall_write (int fd, const void *buffer, size_t size, size_t *count)
  {
    if (fd < 0 || (!buffer && size) || !count || size > (size_t)PTRDIFF_MAX)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__unix__) || defined(__APPLE__)
    ssize_t n = write (fd, buffer, size);
    if (n < 0)
      return -1;
    *count = (size_t)n;
    return 0;
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_syscall_close (int fd)
  {
    if (fd < 0)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__unix__) || defined(__APPLE__)
    return close (fd);
#else
  errno = ENOSYS;
  return -1;
#endif
  }
#ifdef __cplusplus
}
#endif
#endif
