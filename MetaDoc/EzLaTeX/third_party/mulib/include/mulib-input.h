/**
 * @file mulib-input.h
 * @brief Shared input events and Linux evdev reading, without libinput.
 * Define MULIB_IMPL_INPUT once. Native failures preserve errno.
 */
#ifndef MULIB_INPUT_H
#define MULIB_INPUT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Copyable evdev-style event; time_us is in the source clock domain.
   */
  typedef struct mulib_input_event
  {
    uint64_t time_us;
    uint16_t type, code;
    int32_t value;
  } mulib_input_event_t;
  enum
  {
    MULIB_INPUT_SYN = 0,
    MULIB_INPUT_KEY = 1,
    MULIB_INPUT_REL = 2,
    MULIB_INPUT_ABS = 3,
    MULIB_INPUT_KEY_COUNT = 768
  };
  enum
  {
    MULIB_INPUT_OK = 0,
    MULIB_INPUT_EINVAL = -1,
    MULIB_INPUT_EIO = -2,
    MULIB_INPUT_ENOTSUP = -3,
    MULIB_INPUT_ESTATE = -4,
    MULIB_INPUT_AGAIN = 1
  };
  /** @brief Owned evdev fd; initialize, do not copy, close to release any
   * grab. */
  typedef struct mulib_input
  {
    int fd;
  } mulib_input_t;
#define MULIB_INPUT_INIT { -1 }
  /** @brief Check event type <=31, and key code/value bounds. */
  int mulib_input_event_valid (const mulib_input_event_t *event);
  /** @brief Open explicit evdev path, nonblocking and close-on-exec; unchanged
   * on failure. */
  int mulib_input_open (mulib_input_t *device, const char *path);
  /** @brief Close/reset; idempotent. */
  int mulib_input_close (mulib_input_t *device);
  /** @brief Set/clear exclusive EVIOCGRAB; requires enable 0 or 1. */
  int mulib_input_grab (const mulib_input_t *device, int enable);
  /** @brief Read one event; AGAIN on EAGAIN, EIO on EOF/short record. Output
   * unchanged on error. */
  int mulib_input_read (const mulib_input_t *device,
                        mulib_input_event_t *event);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_INPUT) && !defined(MULIB_INPUT_IMPLEMENTED)
#define MULIB_INPUT_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_input_event_valid (const mulib_input_event_t *e)
  {
    return e && e->type <= 31
           && (e->type != MULIB_INPUT_KEY
               || (e->code < MULIB_INPUT_KEY_COUNT && e->value >= 0
                   && e->value <= 2));
  }
  int
  mulib_input_open (mulib_input_t *d, const char *path)
  {
    if (!d || !path)
      return MULIB_INPUT_EINVAL;
    if (d->fd != -1)
      return MULIB_INPUT_ESTATE;
#ifdef __linux__
    {
      int fd = open (path, O_RDONLY | O_NONBLOCK), saved;
      if (fd < 0)
        return MULIB_INPUT_EIO;
      if (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0)
        {
          saved = errno;
          (void)close (fd);
          errno = saved;
          return MULIB_INPUT_EIO;
        }
      d->fd = fd;
      return MULIB_INPUT_OK;
    }
#else
  return MULIB_INPUT_ENOTSUP;
#endif
  }
  int
  mulib_input_close (mulib_input_t *d)
  {
    if (!d)
      return MULIB_INPUT_EINVAL;
    if (d->fd == -1)
      return MULIB_INPUT_OK;
#ifdef __linux__
    {
      int fd = d->fd;
      d->fd = -1;
      return close (fd) < 0 ? MULIB_INPUT_EIO : MULIB_INPUT_OK;
    }
#else
  return MULIB_INPUT_ENOTSUP;
#endif
  }
  int
  mulib_input_grab (const mulib_input_t *d, int enable)
  {
    if (!d || d->fd < 0 || (enable != 0 && enable != 1))
      return MULIB_INPUT_EINVAL;
#ifdef __linux__
    {
      int rc;
      do
        {
          rc = ioctl (d->fd, EVIOCGRAB, enable);
        }
      while (rc < 0 && errno == EINTR);
      return rc < 0 ? MULIB_INPUT_EIO : MULIB_INPUT_OK;
    }
#else
  return MULIB_INPUT_ENOTSUP;
#endif
  }
  int
  mulib_input_read (const mulib_input_t *d, mulib_input_event_t *out)
  {
    if (!d || d->fd < 0 || !out)
      return MULIB_INPUT_EINVAL;
#ifdef __linux__
    {
      struct input_event native;
      mulib_input_event_t e;
      ssize_t n;
      do
        {
          n = read (d->fd, &native, sizeof (native));
        }
      while (n < 0 && errno == EINTR);
      if (n < 0)
        return errno == EAGAIN || errno == EWOULDBLOCK ? MULIB_INPUT_AGAIN
                                                       : MULIB_INPUT_EIO;
      if (n != (ssize_t)sizeof (native))
        {
          errno = EIO;
          return MULIB_INPUT_EIO;
        }
      if ((uint64_t)native.input_event_sec > (UINT64_MAX - 999999) / 1000000
          || (uint64_t)native.input_event_usec > 999999)
        {
          errno = EOVERFLOW;
          return MULIB_INPUT_EIO;
        }
      e.time_us = (uint64_t)native.input_event_sec * 1000000
                  + (uint64_t)native.input_event_usec;
      e.type = native.type;
      e.code = native.code;
      e.value = native.value;
      if (!mulib_input_event_valid (&e))
        {
          errno = EINVAL;
          return MULIB_INPUT_EIO;
        }
      *out = e;
      return MULIB_INPUT_OK;
    }
#else
  return MULIB_INPUT_ENOTSUP;
#endif
  }
#ifdef __cplusplus
}
#endif
#endif
