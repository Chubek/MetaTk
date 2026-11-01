/**
 * @file mulib-udev.h
 * @brief Borrowed Linux kernel uevent decoding and nonblocking netlink
 * monitoring. Not a libudev database/rules implementation. Define UDEV and
 * PARSEC switches.
 */
#ifndef MULIB_UDEV_H
#define MULIB_UDEV_H
#include "mulib-parsec.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_UDEV_OK = 1,
    MULIB_UDEV_AGAIN = 0,
    MULIB_UDEV_INVALID = -1,
    MULIB_UDEV_LIMIT = -2,
    MULIB_UDEV_EIO = -3,
    MULIB_UDEV_ENOTSUP = -4,
    MULIB_UDEV_ESTATE = -5
  };
  /** @brief Borrowed NUL-terminated property; key_length excludes '='. */
  typedef struct mulib_udev_property
  {
    const char *key, *value;
    size_t key_length;
  } mulib_udev_property_t;
  /** @brief Borrowed packet view. action is a span (not terminated); path is
   * terminated. */
  typedef struct mulib_udev_event
  {
    const char *action, *path;
    size_t action_length, count;
    mulib_udev_property_t properties[64];
  } mulib_udev_event_t;
  /** @brief Decode action@/path followed by NUL-separated KEY=value fields;
   * max 64, unique keys, optional trailing NUL padding. Output unchanged on
   * failure.
   */
  int mulib_udev_parse (const void *bytes, size_t length,
                        mulib_udev_event_t *out);
  /** @brief Borrow property value or NULL; packet must still be alive. */
  const char *mulib_udev_get (const mulib_udev_event_t *event,
                              const char *key);
  /** @brief Owned socket; initialize and never copy. */
  typedef struct mulib_udev_monitor
  {
    int fd;
  } mulib_udev_monitor_t;
#define MULIB_UDEV_MONITOR_INIT { -1 }
  /** @brief Subscribe to kernel multicast group 1, nonblocking/close-on-exec.
   */
  int mulib_udev_monitor_open (mulib_udev_monitor_t *monitor);
  /** @brief Close/reset, idempotently. */
  int mulib_udev_monitor_close (mulib_udev_monitor_t *monitor);
  /** @brief Receive into caller storage; reject non-kernel senders/truncated
   * packets. Event borrows buffer on OK. AGAIN means no packet; length is zero
   * on failure.
   */
  int mulib_udev_monitor_read (const mulib_udev_monitor_t *monitor,
                               void *buffer, size_t capacity, size_t *length,
                               mulib_udev_event_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UDEV) && !defined(MULIB_UDEV_IMPLEMENTED)
#define MULIB_UDEV_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <fcntl.h>
#include <linux/netlink.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_udev_match_ (const mulib_parsec_parser_t *p, const char *s, size_t n)
  {
    mulib_parsec_result_t r;
    int ok = mulib_parsec_run (p, s, n, NULL, &r);
    mulib_parsec_result_free (&r);
    return ok == 1;
  }
  const char *
  mulib_udev_get (const mulib_udev_event_t *e, const char *key)
  {
    size_t n;
    if (!e || !key)
      return NULL;
    n = strlen (key);
    for (size_t i = 0; i < e->count; ++i)
      if (e->properties[i].key_length == n
          && !memcmp (e->properties[i].key, key, n))
        return e->properties[i].value;
    return NULL;
  }
  int
  mulib_udev_parse (const void *bytes, size_t n, mulib_udev_event_t *out)
  {
    const char *s = (const char *)bytes, *end, *at;
    size_t pos;
    mulib_udev_event_t e;
    mulib_parsec_parser_t lower = mulib_parsec_range ('a', 'z'),
                          printable = mulib_parsec_range (33, 255);
    mulib_parsec_parser_t header[5], grammar, keys[3], key, field[4],
        field_grammar;
    if (!s || !out || !n || n > 65536)
      return n > 65536 ? MULIB_UDEV_LIMIT : MULIB_UDEV_INVALID;
    end = (const char *)memchr (s, 0, n);
    if (!end)
      return MULIB_UDEV_INVALID;
    at = (const char *)memchr (s, '@', (size_t)(end - s));
    if (!at)
      return MULIB_UDEV_INVALID;
    header[0] = mulib_parsec_repeat (&lower, 1, 32);
    header[1] = mulib_parsec_byte ('@');
    header[2] = mulib_parsec_byte ('/');
    header[3] = mulib_parsec_repeat (&printable, 0, 65536);
    header[4] = mulib_parsec_end ();
    grammar = mulib_parsec_sequence (header, 5);
    if (!mulib_udev_match_ (&grammar, s, (size_t)(end - s)))
      return MULIB_UDEV_INVALID;
    memset (&e, 0, sizeof (e));
    e.action = s;
    e.action_length = (size_t)(at - s);
    e.path = at + 1;
    keys[0] = mulib_parsec_range ('A', 'Z');
    keys[1] = mulib_parsec_range ('0', '9');
    keys[2] = mulib_parsec_byte ('_');
    key = mulib_parsec_choice (keys, 3);
    field[0] = mulib_parsec_repeat (&key, 1, 65536);
    field[1] = mulib_parsec_byte ('=');
    printable = mulib_parsec_range (1, 255);
    field[2] = mulib_parsec_repeat (&printable, 0, 65536);
    field[3] = mulib_parsec_end ();
    field_grammar = mulib_parsec_sequence (field, 4);
    pos = (size_t)(end - s) + 1;
    while (pos < n)
      {
        const char *equals;
        if (!s[pos])
          {
            for (; pos < n; ++pos)
              if (s[pos])
                return MULIB_UDEV_INVALID;
            break;
          }
        end = (const char *)memchr (s + pos, 0, n - pos);
        if (!end)
          return MULIB_UDEV_INVALID;
        if (e.count == 64)
          return MULIB_UDEV_LIMIT;
        if (!mulib_udev_match_ (&field_grammar, s + pos,
                                (size_t)(end - s) - pos))
          return MULIB_UDEV_INVALID;
        equals = (const char *)memchr (s + pos, '=', (size_t)(end - s) - pos);
        for (size_t i = 0; i < e.count; ++i)
          if (e.properties[i].key_length == (size_t)(equals - (s + pos))
              && !memcmp (e.properties[i].key, s + pos,
                          e.properties[i].key_length))
            return MULIB_UDEV_INVALID;
        e.properties[e.count].key = s + pos;
        e.properties[e.count].key_length = (size_t)(equals - (s + pos));
        e.properties[e.count++].value = equals + 1;
        pos = (size_t)(end - s) + 1;
      }
    {
      const char *action = mulib_udev_get (&e, "ACTION"),
                 *path = mulib_udev_get (&e, "DEVPATH");
      if ((action
           && (strlen (action) != e.action_length
               || memcmp (action, e.action, e.action_length)))
          || (path && strcmp (path, e.path)))
        return MULIB_UDEV_INVALID;
    }
    *out = e;
    return MULIB_UDEV_OK;
  }
  int
  mulib_udev_monitor_open (mulib_udev_monitor_t *m)
  {
    if (!m)
      return MULIB_UDEV_INVALID;
    if (m->fd != -1)
      return MULIB_UDEV_ESTATE;
#ifdef __linux__
    {
      struct sockaddr_nl address;
      int fd, error;
      fd = socket (AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
      if (fd < 0)
        return MULIB_UDEV_EIO;
      memset (&address, 0, sizeof (address));
      address.nl_family = AF_NETLINK;
      address.nl_groups = 1;
      if (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0
          || fcntl (fd, F_SETFL, O_NONBLOCK) < 0
          || bind (fd, (struct sockaddr *)&address, sizeof (address)) < 0)
        {
          error = errno;
          (void)close (fd);
          errno = error;
          return MULIB_UDEV_EIO;
        }
      m->fd = fd;
      return MULIB_UDEV_OK;
    }
#else
  return MULIB_UDEV_ENOTSUP;
#endif
  }
  int
  mulib_udev_monitor_close (mulib_udev_monitor_t *m)
  {
    if (!m)
      return MULIB_UDEV_INVALID;
    if (m->fd == -1)
      return MULIB_UDEV_OK;
#ifdef __linux__
    {
      int fd = m->fd;
      m->fd = -1;
      return close (fd) < 0 ? MULIB_UDEV_EIO : MULIB_UDEV_OK;
    }
#else
  return MULIB_UDEV_ENOTSUP;
#endif
  }
  int
  mulib_udev_monitor_read (const mulib_udev_monitor_t *m, void *buffer,
                           size_t cap, size_t *length, mulib_udev_event_t *out)
  {
    if (length)
      *length = 0;
    if (!m || m->fd < 0 || !buffer || !cap || !out)
      return MULIB_UDEV_INVALID;
#ifdef __linux__
    {
      struct sockaddr_nl sender;
      struct iovec iov;
      struct msghdr msg;
      ssize_t n;
      int rc;
      memset (&sender, 0, sizeof (sender));
      memset (&msg, 0, sizeof (msg));
      iov.iov_base = buffer;
      iov.iov_len = cap;
      msg.msg_name = &sender;
      msg.msg_namelen = sizeof (sender);
      msg.msg_iov = &iov;
      msg.msg_iovlen = 1;
      do
        {
          n = recvmsg (m->fd, &msg, 0);
        }
      while (n < 0 && errno == EINTR);
      if (n < 0)
        return errno == EAGAIN || errno == EWOULDBLOCK ? MULIB_UDEV_AGAIN
                                                       : MULIB_UDEV_EIO;
      if (msg.msg_flags & MSG_TRUNC)
        return MULIB_UDEV_LIMIT;
      if (msg.msg_namelen != sizeof (sender) || sender.nl_family != AF_NETLINK
          || sender.nl_pid != 0)
        return MULIB_UDEV_INVALID;
      rc = mulib_udev_parse (buffer, (size_t)n, out);
      if (rc == MULIB_UDEV_OK && length)
        *length = (size_t)n;
      return rc;
    }
#else
  return MULIB_UDEV_ENOTSUP;
#endif
  }
#ifdef __cplusplus
}
#endif
#endif
