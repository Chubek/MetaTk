/**
 * @file mulib-timerfd.h
 * @brief Linux monotonic relative timers, nonblocking and close-on-exec.
 * Define MULIB_IMPL_TIMERFD once. No MuLIB dependencies.
 */
#ifndef MULIB_TIMERFD_H
#define MULIB_TIMERFD_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Create an owned CLOCK_MONOTONIC timer, initially disarmed. */
  int mulib_timerfd_create (void);
  /** @brief Set relative initial/period nanoseconds; initial zero disarms.
   * Return 0/-1. */
  int mulib_timerfd_set (int fd, uint64_t initial_ns, uint64_t interval_ns);
  /** @brief Read expiration count; output unchanged on failure, EAGAIN when
   * not ready. */
  int mulib_timerfd_read (int fd, uint64_t *expirations);
  /** @brief Close once without retry. */
  int mulib_timerfd_close (int fd);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TIMERFD) && !defined(MULIB_TIMERFD_IMPLEMENTED)
#define MULIB_TIMERFD_IMPLEMENTED
#include <errno.h>
#include <time.h>
#if defined(__linux__)
#include <sys/timerfd.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_timerfd_create (void)
  {
#if defined(__linux__)
    /* Linux's CLOCK_MONOTONIC ABI value, even when strict C hides clock
     * macros. */
    return timerfd_create (1, TFD_NONBLOCK | TFD_CLOEXEC);
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_timerfd_set (int fd, uint64_t initial_ns, uint64_t interval_ns)
  {
    if (fd < 0)
      {
        errno = EINVAL;
        return -1;
      }
    /* A deliberately bounded range, representable even with signed 32-bit
     * time_t. */
    if (initial_ns / UINT64_C (1000000000) > INT32_MAX
        || interval_ns / UINT64_C (1000000000) > INT32_MAX)
      {
        errno = EOVERFLOW;
        return -1;
      }
#if defined(__linux__)
    struct itimerspec timer;
    timer.it_value.tv_sec = (time_t)(initial_ns / UINT64_C (1000000000));
    timer.it_value.tv_nsec = (long)(initial_ns % UINT64_C (1000000000));
    timer.it_interval.tv_sec = (time_t)(interval_ns / UINT64_C (1000000000));
    timer.it_interval.tv_nsec = (long)(interval_ns % UINT64_C (1000000000));
    return timerfd_settime (fd, 0, &timer, NULL);
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_timerfd_read (int fd, uint64_t *expirations)
  {
    if (fd < 0 || !expirations)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
    uint64_t count;
    ssize_t n = read (fd, &count, sizeof count);
    if (n < 0)
      return -1;
    if (n != (ssize_t)sizeof count)
      {
        errno = EIO;
        return -1;
      }
    *expirations = count;
    return 0;
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_timerfd_close (int fd)
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
