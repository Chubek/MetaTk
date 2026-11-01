/**
 * @file mulib-fswatch.h
 * @brief Linux inotify watches and alignment-safe native event decoding.
 * Define MULIB_IMPL_FSWATCH once. Decoder also works on non-Linux hosts.
 */
#ifndef MULIB_FSWATCH_H
#define MULIB_FSWATCH_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_FSWATCH_MODIFY = 0x00000002,
    MULIB_FSWATCH_ATTRIB = 0x00000004,
    MULIB_FSWATCH_CLOSE_WRITE = 0x00000008,
    MULIB_FSWATCH_MOVED_FROM = 0x00000040,
    MULIB_FSWATCH_MOVED_TO = 0x00000080,
    MULIB_FSWATCH_CREATE = 0x00000100,
    MULIB_FSWATCH_DELETE = 0x00000200,
    MULIB_FSWATCH_DELETE_SELF = 0x00000400,
    MULIB_FSWATCH_MOVE_SELF = 0x00000800,
    MULIB_FSWATCH_UNMOUNT = 0x00002000,
    MULIB_FSWATCH_OVERFLOW = 0x00004000,
    MULIB_FSWATCH_IGNORED = 0x00008000,
    MULIB_FSWATCH_ISDIR = 0x40000000,
    MULIB_FSWATCH_WATCH_MASK = 0x00000fce
  };
  typedef struct mulib_fswatch_event
  {
    int32_t watch;
    uint32_t mask, cookie;
    const char *name;
    size_t name_length;
  } mulib_fswatch_event;
  /** @brief Create an owned nonblocking, close-on-exec inotify descriptor. */
  int mulib_fswatch_create (void);
  /** @brief Add/replace a nonrecursive path watch using the documented mask
   * subset. */
  int mulib_fswatch_add (int fd, const char *path, unsigned mask);
  /** @brief Remove a watch; its queued IN_IGNORED notification remains
   * readable. */
  int mulib_fswatch_remove (int fd, int watch);
  /** @brief Read a native event batch; count unchanged on error. Minimum
   * capacity 16. */
  int mulib_fswatch_read (int fd, void *buffer, size_t capacity,
                          size_t *count);
  /** @brief Decode one event: 1 event, 0 end, -1 malformed (EINVAL); outputs
   * transactional. */
  int mulib_fswatch_next (const void *buffer, size_t length, size_t *offset,
                          mulib_fswatch_event *event);
  /** @brief Close once, releasing all watches but no filesystem objects. */
  int mulib_fswatch_close (int fd);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FSWATCH) && !defined(MULIB_FSWATCH_IMPLEMENTED)
#define MULIB_FSWATCH_IMPLEMENTED
#include <errno.h>
#include <string.h>
#if defined(__linux__)
#include <sys/inotify.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_fswatch_create (void)
  {
#if defined(__linux__)
    return inotify_init1 (IN_NONBLOCK | IN_CLOEXEC);
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_fswatch_add (int fd, const char *path, unsigned mask)
  {
    if (fd < 0 || !path || !*path || !mask
        || (mask & ~(unsigned)MULIB_FSWATCH_WATCH_MASK))
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
    return inotify_add_watch (fd, path, (uint32_t)mask);
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_fswatch_remove (int fd, int watch)
  {
    if (fd < 0 || watch < 0)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
    return inotify_rm_watch (fd, watch);
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_fswatch_read (int fd, void *buffer, size_t capacity, size_t *count)
  {
    if (fd < 0 || !buffer || !count || capacity < 16
        || capacity > (size_t)PTRDIFF_MAX)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
    ssize_t n = read (fd, buffer, capacity);
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
  mulib_fswatch_next (const void *buffer, size_t length, size_t *offset,
                      mulib_fswatch_event *event)
  {
    const unsigned char *bytes;
    const char *zero;
    uint32_t fields[4];
    mulib_fswatch_event decoded;
    size_t remaining;
    if ((!buffer && length) || length > (size_t)PTRDIFF_MAX || !offset
        || !event || *offset > length)
      {
        errno = EINVAL;
        return -1;
      }
    if (*offset == length)
      return 0;
    remaining = length - *offset;
    if (remaining < 16)
      {
        errno = EINVAL;
        return -1;
      }
    bytes = (const unsigned char *)buffer + *offset;
    memcpy (fields, bytes, 16);
    if ((size_t)fields[3] > remaining - 16)
      {
        errno = EINVAL;
        return -1;
      }
    memcpy (&decoded.watch, bytes, 4);
    if (decoded.watch < -1)
      {
        errno = EINVAL;
        return -1;
      }
    decoded.mask = fields[1];
    decoded.cookie = fields[2];
    decoded.name = NULL;
    decoded.name_length = 0;
    if (fields[3])
      {
        decoded.name = (const char *)(bytes + 16);
        zero = (const char *)memchr (decoded.name, 0, fields[3]);
        if (!zero)
          {
            errno = EINVAL;
            return -1;
          }
        decoded.name_length = (size_t)(zero - decoded.name);
      }
    *event = decoded;
    *offset += 16 + (size_t)fields[3];
    return 1;
  }
  int
  mulib_fswatch_close (int fd)
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
