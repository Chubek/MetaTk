/**
 * @file mulib-vdev.h
 * @brief Linux uinput keyboard/relative-pointer creation and event emission.
 * Define MULIB_IMPL_VDEV and MULIB_IMPL_INPUT once. No implicit input
 * injection.
 */
#ifndef MULIB_VDEV_H
#define MULIB_VDEV_H
#include "mulib-input.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Owned uinput device. Initialize and do not copy; close destroys
   * it. */
  typedef struct mulib_vdev
  {
    int fd;
  } mulib_vdev_t;
#define MULIB_VDEV_INIT { -1 }
  /** @brief Create using explicit path (NULL = /dev/uinput). name is 1..79
   * bytes. keys lists enabled codes <768; relative_axes is a bitmask for REL
   * codes 0..15. At least one capability is required. Returns MULIB_INPUT_*
   * results.
   */
  int mulib_vdev_create (mulib_vdev_t *device, const char *path,
                         const char *name, const uint16_t *keys,
                         size_t key_count, uint16_t relative_axes);
  /** @brief Destroy/close/reset idempotently. Kernel destroys the device on
   * close. */
  int mulib_vdev_close (mulib_vdev_t *device);
  /** @brief Write one SYN/KEY/REL event with timestamp zero (kernel clock).
   * Caller must use enabled codes and send SYN_REPORT to delimit frames.
   * AGAIN on EAGAIN; short writes are EIO. Other event classes are rejected.
   */
  int mulib_vdev_emit (const mulib_vdev_t *device,
                       const mulib_input_event_t *event);
  /** @brief Emit SYN_REPORT. */
  int mulib_vdev_sync (const mulib_vdev_t *device);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VDEV) && !defined(MULIB_VDEV_IMPLEMENTED)
#define MULIB_VDEV_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
#ifdef __linux__
  static int
  mulib_vdev_ioctl_ (int fd, unsigned long request, uintptr_t arg)
  {
    int rc;
    do
      {
        rc = ioctl (fd, request, arg);
      }
    while (rc < 0 && errno == EINTR);
    return rc;
  }
#endif
  int
  mulib_vdev_create (mulib_vdev_t *d, const char *path, const char *name,
                     const uint16_t *keys, size_t count, uint16_t axes)
  {
    size_t len;
    if (!d || !name || (!keys && count) || count > MULIB_INPUT_KEY_COUNT
        || (!count && !axes))
      return MULIB_INPUT_EINVAL;
    len = strlen (name);
    if (!len || len >= 80)
      return MULIB_INPUT_EINVAL;
    for (size_t i = 0; i < count; ++i)
      if (keys[i] >= MULIB_INPUT_KEY_COUNT)
        return MULIB_INPUT_EINVAL;
    if (d->fd != -1)
      return MULIB_INPUT_ESTATE;
#ifdef __linux__
    {
      struct uinput_setup setup;
      int fd = open (path ? path : "/dev/uinput", O_WRONLY | O_NONBLOCK),
          error;
      if (fd < 0)
        return MULIB_INPUT_EIO;
      if (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0)
        goto fail;
      if (count && mulib_vdev_ioctl_ (fd, UI_SET_EVBIT, EV_KEY) < 0)
        goto fail;
      for (size_t i = 0; i < count; ++i)
        if (mulib_vdev_ioctl_ (fd, UI_SET_KEYBIT, keys[i]) < 0)
          goto fail;
      if (axes && mulib_vdev_ioctl_ (fd, UI_SET_EVBIT, EV_REL) < 0)
        goto fail;
      for (unsigned i = 0; i < 16; ++i)
        if ((axes & (1u << i)) && mulib_vdev_ioctl_ (fd, UI_SET_RELBIT, i) < 0)
          goto fail;
      memset (&setup, 0, sizeof (setup));
      memcpy (setup.name, name, len + 1);
      setup.id.bustype = BUS_VIRTUAL;
      setup.id.version = 1;
      if (mulib_vdev_ioctl_ (fd, UI_DEV_SETUP, (uintptr_t)&setup) < 0
          || mulib_vdev_ioctl_ (fd, UI_DEV_CREATE, 0) < 0)
        goto fail;
      d->fd = fd;
      return MULIB_INPUT_OK;
    fail:
      error = errno;
      (void)close (fd);
      errno = error;
      return MULIB_INPUT_EIO;
    }
#else
  (void)path;
  return MULIB_INPUT_ENOTSUP;
#endif
  }
  int
  mulib_vdev_close (mulib_vdev_t *d)
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
  mulib_vdev_emit (const mulib_vdev_t *d, const mulib_input_event_t *e)
  {
    if (!d || d->fd < 0 || !mulib_input_event_valid (e)
        || e->type > MULIB_INPUT_REL
        || (e->type == MULIB_INPUT_REL && e->code > 15)
        || (e->type == MULIB_INPUT_SYN && e->code > 15))
      return MULIB_INPUT_EINVAL;
#ifdef __linux__
    {
      struct input_event native;
      ssize_t n;
      memset (&native, 0, sizeof (native));
      native.type = e->type;
      native.code = e->code;
      native.value = e->value;
      do
        {
          n = write (d->fd, &native, sizeof (native));
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
      return MULIB_INPUT_OK;
    }
#else
  return MULIB_INPUT_ENOTSUP;
#endif
  }
  int
  mulib_vdev_sync (const mulib_vdev_t *d)
  {
    mulib_input_event_t e = { 0, MULIB_INPUT_SYN, 0, 0 };
    return mulib_vdev_emit (d, &e);
  }
#ifdef __cplusplus
}
#endif
#endif
