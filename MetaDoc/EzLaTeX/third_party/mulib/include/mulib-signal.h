/**
 * @file mulib-signal.h
 * @brief Scoped ISO C signal dispositions; no unsafe callback dispatcher.
 * Define MULIB_IMPL_SIGNAL once. See the manual for handler restrictions.
 */
#ifndef MULIB_SIGNAL_H
#define MULIB_SIGNAL_H
#include <signal.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef void (*mulib_signal_handler) (int);
  typedef struct mulib_signal_guard
  {
    int number;
    mulib_signal_handler previous;
    int active;
  } mulib_signal_guard;
  /** @brief Install a handler/SIG_DFL/SIG_IGN in a zero-initialized inactive
   * guard. */
  int mulib_signal_install (mulib_signal_guard *guard, int number,
                            mulib_signal_handler handler);
  /** @brief Restore the saved disposition; inactive guards are an idempotent
   * success. */
  int mulib_signal_restore (mulib_signal_guard *guard);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SIGNAL) && !defined(MULIB_SIGNAL_IMPLEMENTED)
#define MULIB_SIGNAL_IMPLEMENTED
#include <errno.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_signal_install (mulib_signal_guard *guard, int number,
                        mulib_signal_handler handler)
  {
    mulib_signal_handler previous;
    if (!guard || number <= 0 || handler == SIG_ERR)
      {
        errno = EINVAL;
        return -1;
      }
    if (guard->active)
      {
        errno = EBUSY;
        return -1;
      }
#ifdef SIGKILL
    if (number == SIGKILL)
      {
        errno = EINVAL;
        return -1;
      }
#endif
#ifdef SIGSTOP
    if (number == SIGSTOP)
      {
        errno = EINVAL;
        return -1;
      }
#endif
    /* ISO C need not set errno on failure: provide a deterministic fallback.
     */
    {
      int saved = errno;
      errno = 0;
      previous = signal (number, handler);
      if (previous == SIG_ERR)
        {
          if (!errno)
            errno = EINVAL;
          return -1;
        }
      errno = saved;
    }
    guard->number = number;
    guard->previous = previous;
    guard->active = 1;
    return 0;
  }
  int
  mulib_signal_restore (mulib_signal_guard *guard)
  {
    int saved;
    if (!guard)
      {
        errno = EINVAL;
        return -1;
      }
    if (!guard->active)
      return 0;
    saved = errno;
    errno = 0;
    if (signal (guard->number, guard->previous) == SIG_ERR)
      {
        if (!errno)
          errno = EINVAL;
        return -1;
      }
    errno = saved;
    guard->active = 0;
    return 0;
  }
#ifdef __cplusplus
}
#endif
#endif
