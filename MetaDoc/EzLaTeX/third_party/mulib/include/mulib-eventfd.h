/**
 * @file mulib-eventfd.h
 * @brief Nonblocking close-on-exec Linux eventfd counters.
 * Define MULIB_IMPL_EVENTFD once. Native failures use errno, unsupported
 * ENOSYS.
 */
#ifndef MULIB_EVENTFD_H
#define MULIB_EVENTFD_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Create an owned counter; semaphore must be 0 or 1. */
  int mulib_eventfd_create (uint32_t initial, int semaphore);
  /** @brief Read/drain the counter (or one semaphore unit); output unchanged
   * on error. */
  int mulib_eventfd_read (int fd, uint64_t *value);
  /** @brief Add a value other than UINT64_MAX; return 0/-1 (EAGAIN on
   * overflow). */
  int mulib_eventfd_write (int fd, uint64_t value);
  /** @brief Close once without retry, returning native 0/-1. */
  int mulib_eventfd_close (int fd);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EVENTFD) && !defined(MULIB_EVENTFD_IMPLEMENTED)
#define MULIB_EVENTFD_IMPLEMENTED
#include <errno.h>
#if defined(__linux__)
#include <sys/eventfd.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_eventfd_create (uint32_t initial, int semaphore)
  {
    if (semaphore != 0 && semaphore != 1)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
    return eventfd (initial, EFD_CLOEXEC | EFD_NONBLOCK
                                 | (semaphore ? EFD_SEMAPHORE : 0));
#else
  (void)initial;
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_eventfd_read (int fd, uint64_t *value)
  {
    if (fd < 0 || !value)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
    uint64_t temporary;
    ssize_t n = read (fd, &temporary, sizeof temporary);
    if (n < 0)
      return -1;
    if (n != (ssize_t)sizeof temporary)
      {
        errno = EIO;
        return -1;
      }
    *value = temporary;
    return 0;
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_eventfd_write (int fd, uint64_t value)
  {
    if (fd < 0 || value == UINT64_MAX)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
    ssize_t n = write (fd, &value, sizeof value);
    if (n < 0)
      return -1;
    if (n != (ssize_t)sizeof value)
      {
        errno = EIO;
        return -1;
      }
    return 0;
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_eventfd_close (int fd)
  {
    if (fd < 0)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
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
