/**
 * @file mulib-perf.h
 * @brief Checked native monotonic nanosecond timing (not hardware counters).
 * Define MULIB_IMPL_PERF once. Unsupported clock backends return ENOSYS.
 */
#ifndef MULIB_PERF_H
#define MULIB_PERF_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Read monotonic nanoseconds; output unchanged on failure. */
  int mulib_perf_now (uint64_t *nanoseconds);
  /** @brief Subtract ordered timestamps, rejecting backwards input with
   * EINVAL. */
  int mulib_perf_elapsed (uint64_t begin, uint64_t end, uint64_t *nanoseconds);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PERF) && !defined(MULIB_PERF_IMPLEMENTED)
#define MULIB_PERF_IMPLEMENTED
#include <errno.h>
#include <time.h>
/* Linux clockid_t is int. Supply its libc ABI declaration only if strict C
 * hid the POSIX declaration; never define feature macros after user headers.
 */
#if defined(__linux__) && defined(__LP64__) && !defined(CLOCK_MONOTONIC)
#ifdef __cplusplus
extern "C" int clock_gettime (int, struct timespec *)
#if defined(__GLIBC__)
    noexcept
#endif
    ;
#else
extern int clock_gettime (int, struct timespec *);
#endif
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_perf_now (uint64_t *nanoseconds)
  {
    if (!nanoseconds)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(CLOCK_MONOTONIC) || (defined(__linux__) && defined(__LP64__))
    struct timespec now;
    uint64_t seconds;
#if defined(CLOCK_MONOTONIC)
    if (clock_gettime (CLOCK_MONOTONIC, &now) < 0)
      return -1;
#else
    if (clock_gettime (1, &now) < 0)
      return -1;
#endif
    if (now.tv_sec < 0 || now.tv_nsec < 0 || now.tv_nsec >= 1000000000L)
      {
        errno = EIO;
        return -1;
      }
    seconds = (uint64_t)now.tv_sec;
    if (seconds > (UINT64_MAX - (uint64_t)now.tv_nsec) / UINT64_C (1000000000))
      {
        errno = EOVERFLOW;
        return -1;
      }
    *nanoseconds = seconds * UINT64_C (1000000000) + (uint64_t)now.tv_nsec;
    return 0;
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_perf_elapsed (uint64_t begin, uint64_t end, uint64_t *nanoseconds)
  {
    if (!nanoseconds || end < begin)
      {
        errno = EINVAL;
        return -1;
      }
    *nanoseconds = end - begin;
    return 0;
  }
#ifdef __cplusplus
}
#endif
#endif
