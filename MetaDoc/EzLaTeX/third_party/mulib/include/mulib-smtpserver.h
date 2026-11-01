/** @file mulib-smtpserver.h
 * @brief Allocation-free bounded SMTP envelope and DATA receiver.
 * Define MULIB_IMPL_SMTPSERVER once. No implicit dependencies or TLS.
 * See manual/mulib-smtpserver.md for the deliberately restricted grammar.
 */
#ifndef MULIB_SMTPSERVER_H
#define MULIB_SMTPSERVER_H
#include <stddef.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_smtpserver mulib_smtpserver;
  /** Borrowed callbacks; send is all-or-fail. envelope validates each
   * MAIL/RCPT path (recipient is 0/1); message inspects session
   * sender/recipients and bytes. Return nonzero to accept. No callback may
   * reenter a session. */
  typedef struct mulib_smtpserver_io
  {
    int (*send) (void *, const void *, size_t);
    int (*envelope) (void *, int recipient, const char *path);
    int (*message) (void *, const mulib_smtpserver *, const unsigned char *,
                    size_t);
  } mulib_smtpserver_io;
  /** Fixed maxima: 16 recipients, 255 path bytes. Caller-owned line and
   * message buffers must be disjoint and live throughout the session. */
  struct mulib_smtpserver
  {
    mulib_smtpserver_io io;
    void *context;
    char *line;
    unsigned char *message;
    size_t capacity, length, message_capacity, message_length, recipient_count;
    char sender[256], recipients[16][256];
    int cr, failed, closed, greeted, mail, data, rejected, overflow;
  };
  /** Validate focused 7-bit DATA content: printable ASCII/tab, strict CRLF,
   * complete final CRLF (or empty), at most line_limit bytes per unstuffed
   * line. NULL allowed at n=0. Checks n <= limit. Shared by the SMTP client.
   */
  int mulib_smtp_message_valid (const void *data, size_t n, size_t limit,
                                size_t line_limit);
  /** Initialize and emit greeting. line capacity >= 8, message may be NULL
   * only at zero capacity. No allocations; borrowed buffers may not alias. */
  int mulib_smtpserver_init (mulib_smtpserver *s, char *line, size_t capacity,
                             void *message, size_t message_capacity,
                             const mulib_smtpserver_io *io, void *context);
  /** Consume incremental strict CRLF input. NULL only for zero n. Returns 1 or
   * terminal 0 on framing, command overflow, or output failure. DATA overflow
   * or invalid octets are drained through terminator and rejected, never
   * delivered. Unsupported commands get 502, syntax/state errors 501/503. */
  int mulib_smtpserver_feed (mulib_smtpserver *s, const void *data, size_t n);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SMTPSERVER) && !defined(MULIB_SMTPSERVER_IMPLEMENTED)
#define MULIB_SMTPSERVER_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_smtp_message_valid (const void *data, size_t n, size_t limit,
                            size_t line_limit)
  {
    const unsigned char *p = (const unsigned char *)data;
    size_t column = 0;
    if ((!data && n) || n > limit)
      return 0;
    for (size_t i = 0; i < n; ++i)
      {
        if (p[i] == '\r')
          {
            if (i + 1 >= n || p[++i] != '\n')
              return 0;
            column = 0;
          }
        else
          {
            if ((p[i] < 32 && p[i] != '\t') || p[i] > 126
                || column >= line_limit)
              return 0;
            ++column;
          }
      }
    return !n || (n >= 2 && p[n - 2] == '\r' && p[n - 1] == '\n');
  }
  static int
  mulib_smtpserver_send_ (mulib_smtpserver *s, const char *text)
  {
    if (!s->io.send (s->context, text, strlen (text)))
      {
        s->failed = 1;
        return 0;
      }
    return 1;
  }
  static int
  mulib_smtpserver_equal_ (const char *a, const char *b)
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
  static void
  mulib_smtpserver_reset_ (mulib_smtpserver *s)
  {
    s->mail = 0;
    s->recipient_count = 0;
    s->sender[0] = 0;
    s->data = 0;
    s->message_length = 0;
    s->rejected = 0;
  }
  static int
  mulib_smtpserver_path_ (const char *s, const char *prefix, char out[256],
                          int empty)
  {
    size_t k = strlen (prefix), n = strlen (s), len;
    char head[6];
    if (k >= sizeof (head) || n < k + 2)
      return 0;
    memcpy (head, s, k);
    head[k] = 0;
    if (!mulib_smtpserver_equal_ (head, prefix) || s[k] != '<'
        || s[n - 1] != '>')
      return 0;
    len = n - k - 2;
    if (len >= 256 || (!len && !empty))
      return 0;
    for (size_t i = k + 1; i < n - 1; ++i)
      {
        unsigned char c = (unsigned char)s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
              || (c >= '0' && c <= '9')
              || strchr ("!#$%&'*+-/=?^_`{|}~.@", c)))
          return 0;
      }
    memcpy (out, s + k + 1, len);
    out[len] = 0;
    return 1;
  }
  static int
  mulib_smtpserver_command_ (mulib_smtpserver *s)
  {
    char *verb = s->line, *arg = strchr (verb, ' '), path[256];
    int recipient;
    if (arg)
      *arg++ = 0;
    else
      arg = verb + strlen (verb);
    if (mulib_smtpserver_equal_ (verb, "QUIT") && !*arg)
      {
        s->closed = 1;
        return mulib_smtpserver_send_ (s, "221 goodbye\r\n");
      }
    if (mulib_smtpserver_equal_ (verb, "NOOP") && !*arg)
      return mulib_smtpserver_send_ (s, "250 OK\r\n");
    if (mulib_smtpserver_equal_ (verb, "RSET") && !*arg)
      {
        mulib_smtpserver_reset_ (s);
        return mulib_smtpserver_send_ (s, "250 reset\r\n");
      }
    if (mulib_smtpserver_equal_ (verb, "HELO")
        || mulib_smtpserver_equal_ (verb, "EHLO"))
      {
        if (!*arg || strlen (arg) > 255 || strchr (arg, ' ')
            || strchr (arg, '\t'))
          return mulib_smtpserver_send_ (s, "501 invalid greeting\r\n");
        mulib_smtpserver_reset_ (s);
        s->greeted = 1;
        return mulib_smtpserver_send_ (s, "250 MuLIB\r\n");
      }
    if (mulib_smtpserver_equal_ (verb, "DATA"))
      {
        if (*arg)
          return mulib_smtpserver_send_ (s, "501 DATA takes no arguments\r\n");
        if (!s->mail || !s->recipient_count)
          return mulib_smtpserver_send_ (s, "503 envelope required\r\n");
        s->data = 1;
        s->message_length = 0;
        s->rejected = 0;
        return mulib_smtpserver_send_ (s, "354 end with dot\r\n");
      }
    recipient = mulib_smtpserver_equal_ (verb, "RCPT");
    if (!recipient && !mulib_smtpserver_equal_ (verb, "MAIL"))
      return mulib_smtpserver_send_ (s, "502 unsupported command\r\n");
    if (!s->greeted || (recipient && !s->mail))
      return mulib_smtpserver_send_ (s, "503 bad sequence\r\n");
    if (!recipient)
      mulib_smtpserver_reset_ (s);
    if (!mulib_smtpserver_path_ (arg, recipient ? "TO:" : "FROM:", path,
                                 !recipient))
      return mulib_smtpserver_send_ (
          s, "501 invalid path or unsupported parameter\r\n");
    if (recipient && s->recipient_count == 16)
      return mulib_smtpserver_send_ (s, "452 too many recipients\r\n");
    if (!s->io.envelope || !s->io.envelope (s->context, recipient, path))
      return mulib_smtpserver_send_ (s, "550 envelope denied\r\n");
    if (recipient)
      {
        strcpy (s->recipients[s->recipient_count], path);
        ++s->recipient_count;
      }
    else
      {
        strcpy (s->sender, path);
        s->mail = 1;
      }
    return mulib_smtpserver_send_ (s, "250 accepted\r\n");
  }
  static int
  mulib_smtpserver_data_ (mulib_smtpserver *s)
  {
    size_t offset = s->length && s->line[0] == '.' ? 1 : 0;
    if (!s->overflow && s->length == 1 && s->line[0] == '.')
      {
        int rejection = s->rejected;
        int accepted
            = !rejection && s->io.message
              && s->io.message (s->context, s, s->message, s->message_length);
        mulib_smtpserver_reset_ (s);
        return mulib_smtpserver_send_ (
            s, rejection == 552 ? "552 message too large\r\n"
               : rejection      ? "554 invalid DATA\r\n"
               : accepted       ? "250 queued\r\n"
                                : "554 message denied\r\n");
      }
    /* A leading dot must have been doubled by the peer. */
    if (offset && (s->length < 2 || s->line[1] != '.'))
      s->rejected = 554;
    if (!s->rejected)
      {
        size_t n = s->length - offset;
        if (n > s->message_capacity - s->message_length
            || s->message_capacity - s->message_length - n < 2)
          s->rejected = 552;
        else
          {
            if (n)
              memcpy (s->message + s->message_length, s->line + offset, n);
            s->message_length += n;
            s->message[s->message_length++] = '\r';
            s->message[s->message_length++] = '\n';
          }
      }
    return 1;
  }
  int
  mulib_smtpserver_init (mulib_smtpserver *s, char *line, size_t capacity,
                         void *message, size_t message_capacity,
                         const mulib_smtpserver_io *io, void *context)
  {
    mulib_smtpserver_io callbacks;
    if (!s || !line || capacity < 8 || (!message && message_capacity) || !io
        || !io->send)
      return 0;
    callbacks = *io;
    memset (s, 0, sizeof (*s));
    s->line = line;
    s->capacity = capacity;
    s->message = (unsigned char *)message;
    s->message_capacity = message_capacity;
    s->io = callbacks;
    s->context = context;
    return mulib_smtpserver_send_ (s, "220 MuLIB SMTP subset\r\n");
  }
  int
  mulib_smtpserver_feed (mulib_smtpserver *s, const void *data, size_t n)
  {
    const unsigned char *p = (const unsigned char *)data;
    if (!s || (!data && n) || s->failed || s->closed)
      return 0;
    for (size_t i = 0; i < n; ++i)
      {
        unsigned char c = p[i];
        if (s->closed)
          {
            s->failed = 1;
            return 0;
          }
        if (s->cr)
          {
            if (c != '\n')
              {
                s->failed = 1;
                return 0;
              }
            s->cr = 0;
            s->line[s->length] = 0;
            if (!(s->data ? mulib_smtpserver_data_ (s)
                          : mulib_smtpserver_command_ (s)))
              return 0;
            s->length = 0;
            s->overflow = 0;
          }
        else if (c == '\r')
          s->cr = 1;
        else
          {
            if (c == '\n')
              {
                s->failed = 1;
                return 0;
              }
            if ((c < 32 && c != '\t') || c > 126 || (!s->data && c == '\t'))
              {
                if (!s->data)
                  {
                    s->failed = 1;
                    return 0;
                  }
                s->rejected = 554;
              }
            if (s->length >= s->capacity - 1)
              {
                if (!s->data)
                  {
                    s->failed = 1;
                    return 0;
                  }
                s->overflow = 1;
                if (!s->rejected)
                  s->rejected = 552;
              }
            else
              s->line[s->length++] = (char)c;
          }
      }
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
