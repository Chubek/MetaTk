/**
 * @file mulib-ftpserver.h
 * @brief Bounded, transport-independent FTP control and binary transfer
 * session. Define MULIB_IMPL_FTPSERVER in one C translation unit. No
 * dependencies. See manual/mulib-ftpserver.md for the focused command set and
 * contracts.
 */
#ifndef MULIB_FTPSERVER_H
#define MULIB_FTPSERVER_H
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** Callbacks borrow all arguments for the call. send must accept all bytes
   * or return zero. command opens RETR/STOR resources; close releases them
   * exactly once. read returns 1 with *written <= capacity (zero means EOF).
   * No reentry. command PWD writes a NUL-terminated printable path without
   * quotes. */
  typedef struct mulib_ftpserver_io
  {
    int (*send) (void *, const void *, size_t);
    int (*authorize) (void *, const char *, const char *);
    int (*command) (void *, const char *, const char *, char *, size_t);
    int (*read) (void *, void *, size_t, size_t *);
    int (*write) (void *, const void *, size_t);
    void (*close) (void *, int);
  } mulib_ftpserver_io;
  /** Public storage; fields are observational, not caller-mutable after init.
   */
  typedef struct mulib_ftpserver
  {
    mulib_ftpserver_io io;
    void *context;
    char *line;
    size_t capacity, length;
    char user[128];
    int cr, failed, closed, authorized, transfer;
  } mulib_ftpserver;
  /** Initialize and emit 220. Borrow line[capacity], capacity >= 2, and
   * context. No allocation. Failure is terminal; invalid arguments do not
   * initialize s. */
  int mulib_ftpserver_init (mulib_ftpserver *s, char *line, size_t capacity,
                            const mulib_ftpserver_io *io, void *context);
  /** Incrementally consume strict CRLF control lines. NULL data only if n ==
   * 0. Returns 1 or terminal 0; framing/size/output failures poison the
   * session. Protocol/state errors instead emit 4xx/5xx and remain usable. */
  int mulib_ftpserver_feed (mulib_ftpserver *s, const void *data, size_t n);
  /** Deliver binary STOR data; NULL permitted only for zero bytes. */
  int mulib_ftpserver_write (mulib_ftpserver *s, const void *data, size_t n);
  /** Obtain binary RETR data; out non-NULL, capacity > 0, written non-NULL.
   * EOF does not close the transfer: call finish. Invalid calls return zero.
   */
  int mulib_ftpserver_read (mulib_ftpserver *s, void *out, size_t capacity,
                            size_t *written);
  /** Close a transfer and emit 226 or 451. Call with success=0 to abort. */
  int mulib_ftpserver_finish (mulib_ftpserver *s, int success);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_FTPSERVER) && !defined(MULIB_FTPSERVER_IMPLEMENTED)
#define MULIB_FTPSERVER_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_ftpserver_fail_ (mulib_ftpserver *s)
  {
    s->failed = 1;
    if (s->transfer)
      {
        s->transfer = 0;
        if (s->io.close)
          s->io.close (s->context, 0);
      }
    return 0;
  }
  static int
  mulib_ftpserver_send_ (mulib_ftpserver *s, const char *text)
  {
    if (!s->io.send (s->context, text, strlen (text)))
      return mulib_ftpserver_fail_ (s);
    return 1;
  }
  static int
  mulib_ftpserver_equal_ (const char *a, const char *b)
  {
    while (*a && *b)
      {
        unsigned char c = (unsigned char)*a++;
        if (c >= 'a' && c <= 'z')
          c = (unsigned char)(c - 'a' + 'A');
        if (c != (unsigned char)*b++)
          return 0;
      }
    return !*a && !*b;
  }
  static int
  mulib_ftpserver_command_ (mulib_ftpserver *s)
  {
    char *verb = s->line, *arg = strchr (verb, ' ');
    char result[256];
    int is_retr, is_stor;
    if (arg)
      *arg++ = 0;
    else
      arg = verb + strlen (verb);
    if (!*verb || (strchr (verb, '\t') != NULL))
      return mulib_ftpserver_send_ (s, "500 malformed command\r\n");
    if (s->transfer)
      return mulib_ftpserver_send_ (s, "450 transfer active\r\n");
    if (mulib_ftpserver_equal_ (verb, "QUIT") && !*arg)
      {
        s->closed = 1;
        return mulib_ftpserver_send_ (s, "221 goodbye\r\n");
      }
    if (mulib_ftpserver_equal_ (verb, "NOOP") && !*arg)
      return mulib_ftpserver_send_ (s, "200 OK\r\n");
    if (mulib_ftpserver_equal_ (verb, "USER"))
      {
        size_t n = strlen (arg);
        s->authorized = 0;
        s->user[0] = 0;
        if (!n || n >= sizeof (s->user))
          return mulib_ftpserver_send_ (s, "501 invalid user\r\n");
        memcpy (s->user, arg, n + 1);
        return mulib_ftpserver_send_ (s, "331 password required\r\n");
      }
    if (mulib_ftpserver_equal_ (verb, "PASS"))
      {
        if (!s->user[0])
          return mulib_ftpserver_send_ (s, "503 USER required\r\n");
        s->authorized
            = s->io.authorize && s->io.authorize (s->context, s->user, arg);
        s->user[0] = 0;
        return mulib_ftpserver_send_ (s, s->authorized ? "230 logged in\r\n"
                                                       : "530 denied\r\n");
      }
    is_retr = mulib_ftpserver_equal_ (verb, "RETR");
    is_stor = mulib_ftpserver_equal_ (verb, "STOR");
    if (!is_retr && !is_stor && !mulib_ftpserver_equal_ (verb, "PWD")
        && !mulib_ftpserver_equal_ (verb, "CWD")
        && !mulib_ftpserver_equal_ (verb, "DELE")
        && !mulib_ftpserver_equal_ (verb, "TYPE"))
      return mulib_ftpserver_send_ (s, "502 unsupported command\r\n");
    if (!s->authorized)
      return mulib_ftpserver_send_ (s, "530 login required\r\n");
    if (mulib_ftpserver_equal_ (verb, "TYPE"))
      return mulib_ftpserver_send_ (s, mulib_ftpserver_equal_ (arg, "I")
                                           ? "200 binary mode\r\n"
                                           : "504 only TYPE I supported\r\n");
    if (mulib_ftpserver_equal_ (verb, "PWD") ? *arg != 0 : !*arg)
      return mulib_ftpserver_send_ (s, "501 invalid argument\r\n");
    if ((is_retr && !s->io.read) || (is_stor && !s->io.write)
        || ((is_retr || is_stor) && !s->io.close))
      return mulib_ftpserver_send_ (s, "425 no data adapter\r\n");
    /* Callbacks always receive canonical uppercase operation names. */
    for (char *p = verb; *p; ++p)
      if (*p >= 'a' && *p <= 'z')
        *p = (char)(*p - 'a' + 'A');
    memset (result, 0, sizeof (result));
    if (!s->io.command
        || !s->io.command (s->context, verb, arg, result, sizeof (result)))
      return mulib_ftpserver_send_ (s, "550 operation failed\r\n");
    if (is_retr || is_stor)
      {
        s->transfer = is_stor ? 2 : 1;
        return mulib_ftpserver_send_ (s, "150 data transfer\r\n");
      }
    if (mulib_ftpserver_equal_ (verb, "PWD"))
      {
        size_t i;
        char reply[280];
        for (i = 0; i < sizeof (result) && result[i]; ++i)
          if ((unsigned char)result[i] < 32 || (unsigned char)result[i] > 126
              || result[i] == '"')
            return mulib_ftpserver_send_ (s, "451 invalid storage result\r\n");
        if (i == sizeof (result))
          return mulib_ftpserver_send_ (s, "451 invalid storage result\r\n");
        (void)snprintf (reply, sizeof (reply), "257 \"%s\"\r\n", result);
        return mulib_ftpserver_send_ (s, reply);
      }
    return mulib_ftpserver_send_ (s, "250 completed\r\n");
  }
  int
  mulib_ftpserver_init (mulib_ftpserver *s, char *line, size_t capacity,
                        const mulib_ftpserver_io *io, void *context)
  {
    mulib_ftpserver_io callbacks;
    if (!s || !line || capacity < 2 || !io || !io->send)
      return 0;
    callbacks = *io;
    memset (s, 0, sizeof (*s));
    s->line = line;
    s->capacity = capacity;
    s->io = callbacks;
    s->context = context;
    return mulib_ftpserver_send_ (s, "220 MuLIB FTP subset\r\n");
  }
  int
  mulib_ftpserver_feed (mulib_ftpserver *s, const void *data, size_t n)
  {
    const unsigned char *p = (const unsigned char *)data;
    if (!s || (!data && n) || s->failed || s->closed)
      return 0;
    for (size_t i = 0; i < n; ++i)
      {
        unsigned char c = p[i];
        if (s->closed)
          return mulib_ftpserver_fail_ (s);
        if (s->cr)
          {
            if (c != '\n')
              return mulib_ftpserver_fail_ (s);
            s->cr = 0;
            s->line[s->length] = 0;
            s->length = 0;
            if (!mulib_ftpserver_command_ (s))
              return 0;
          }
        else if (c == '\r')
          s->cr = 1;
        else
          {
            if (c < 32 || c > 126 || s->length >= s->capacity - 1)
              return mulib_ftpserver_fail_ (s);
            s->line[s->length++] = (char)c;
          }
      }
    return 1;
  }
  int
  mulib_ftpserver_write (mulib_ftpserver *s, const void *data, size_t n)
  {
    if (!s || s->failed || s->closed || s->transfer != 2 || (!data && n))
      return 0;
    if (n && !s->io.write (s->context, data, n))
      return mulib_ftpserver_finish (s, 0);
    return 1;
  }
  int
  mulib_ftpserver_read (mulib_ftpserver *s, void *out, size_t capacity,
                        size_t *written)
  {
    size_t n = 0;
    if (!s || s->failed || s->closed || s->transfer != 1 || !out || !capacity
        || !written)
      return 0;
    if (!s->io.read (s->context, out, capacity, &n) || n > capacity)
      {
        (void)mulib_ftpserver_finish (s, 0);
        return 0;
      }
    *written = n;
    return 1;
  }
  int
  mulib_ftpserver_finish (mulib_ftpserver *s, int success)
  {
    if (!s || s->failed || s->closed || !s->transfer)
      return 0;
    s->transfer = 0;
    s->io.close (s->context, success != 0);
    if (!mulib_ftpserver_send_ (s, success ? "226 transfer complete\r\n"
                                           : "451 transfer failed\r\n"))
      return 0;
    return success != 0;
  }
#ifdef __cplusplus
}
#endif
#endif
