/** @file mulib-smtpclient.h
 * @brief Bounded SMTP envelope/session client with strict DATA dot stuffing.
 * Depends on mulib-smtpserver.h's shared message validator. Define both
 * MULIB_IMPL_SMTPSERVER and MULIB_IMPL_SMTPCLIENT once, explicitly.
 */
#ifndef MULIB_SMTPCLIENT_H
#define MULIB_SMTPCLIENT_H
#include "mulib-smtpserver.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_smtpclient_command
  {
    MULIB_SMTP_HELO,
    MULIB_SMTP_MAIL,
    MULIB_SMTP_RCPT,
    MULIB_SMTP_DATA,
    MULIB_SMTP_RSET,
    MULIB_SMTP_NOOP,
    MULIB_SMTP_QUIT
  } mulib_smtpclient_command;
  /** state: 0 greeting, 1 ready, 2 reply pending, 3 DATA ready, 4 closed, -1
   * failed. code is last completed reply; greeted/mail/recipients track
   * accepted envelope. */
  typedef struct mulib_smtpclient
  {
    int (*send) (void *, const void *, size_t);
    void *context;
    char *line;
    size_t capacity, length, message_limit, line_limit, recipients;
    int cr, state, code, multiline, greeted, mail, completing;
    mulib_smtpclient_command pending;
  } mulib_smtpclient;
  /** Borrow line[capacity >= 8]. Limits are message bytes before stuffing and
   * unstuffed bytes per line. send consumes all bytes or returns zero; no
   * reentry. */
  int mulib_smtpclient_init (mulib_smtpclient *c, char *line, size_t capacity,
                             size_t message_limit, size_t line_limit,
                             int (*send) (void *, const void *, size_t),
                             void *context);
  /** Incremental replies; strict CRLF and same-code multiline SMTP replies.
   * Invalid input/overflow is terminal. NULL data only at n=0. */
  int mulib_smtpclient_feed (mulib_smtpclient *c, const void *data, size_t n);
  /** Submit command with bare path for MAIL/RCPT, hostname for HELO,
   * NULL/empty for others. Empty MAIL means reverse-path <>. Validation/state
   * failures send nothing and leave the client usable. Return 1 success, 0
   * failure. */
  int mulib_smtpclient_request (mulib_smtpclient *c,
                                mulib_smtpclient_command command,
                                const char *arg);
  /** In state 3, validate complete bounded CRLF message before sending any
   * bytes, dot-stuff and terminate it. Empty message allowed, NULL only at
   * zero length. No allocation; callback failure may leave partial output and
   * is terminal. */
  int mulib_smtpclient_message (mulib_smtpclient *c, const void *data,
                                size_t n);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SMTPCLIENT) && !defined(MULIB_SMTPCLIENT_IMPLEMENTED)
#define MULIB_SMTPCLIENT_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_smtpclient_init (mulib_smtpclient *c, char *line, size_t capacity,
                         size_t message_limit, size_t line_limit,
                         int (*send) (void *, const void *, size_t),
                         void *context)
  {
    if (!c || !line || capacity < 8 || !send)
      return 0;
    memset (c, 0, sizeof (*c));
    c->line = line;
    c->capacity = capacity;
    c->message_limit = message_limit;
    c->line_limit = line_limit;
    c->send = send;
    c->context = context;
    return 1;
  }
  static int
  mulib_smtpclient_reply_ (mulib_smtpclient *c)
  {
    const char *p = c->line;
    int code;
    if (c->state != 0 && c->state != 2)
      return 0;
    if (c->length < 4 || p[0] < '2' || p[0] > '5' || p[1] < '0' || p[1] > '9'
        || p[2] < '0' || p[2] > '9' || (p[3] != ' ' && p[3] != '-'))
      return 0;
    code = (p[0] - '0') * 100 + (p[1] - '0') * 10 + p[2] - '0';
    if (c->multiline && code != c->multiline)
      return 0;
    if (p[3] == '-')
      {
        c->multiline = code;
        return 1;
      }
    c->multiline = 0;
    c->code = code;
    if (c->state == 0)
      {
        if (code != 220)
          return 0;
        c->state = 1;
        return 1;
      }
    if (c->state != 2)
      return 0;
    if (code == 421)
      {
        c->state = 4;
        return 1;
      }
    if (c->completing)
      {
        if (code != 250 && code < 400)
          return 0;
        c->completing = 0;
        c->mail = 0;
        c->recipients = 0;
        c->state = 1;
        return 1;
      }
    if (c->pending == MULIB_SMTP_DATA && code == 354)
      {
        c->state = 3;
        return 1;
      }
    if (code < 200 || (code >= 300 && code < 400))
      return 0;
    c->state = 1;
    if (code >= 400)
      return 1;
    if (c->pending == MULIB_SMTP_QUIT ? code != 221
        : c->pending == MULIB_SMTP_RCPT
            ? code != 250 && code != 251 && code != 252
            : code != 250)
      return 0;
    switch (c->pending)
      {
      case MULIB_SMTP_HELO:
        c->greeted = 1;
        c->mail = 0;
        c->recipients = 0;
        break;
      case MULIB_SMTP_MAIL:
        c->mail = 1;
        c->recipients = 0;
        break;
      case MULIB_SMTP_RCPT:
        ++c->recipients;
        break;
      case MULIB_SMTP_RSET:
        c->mail = 0;
        c->recipients = 0;
        break;
      case MULIB_SMTP_QUIT:
        c->state = 4;
        break;
      case MULIB_SMTP_DATA:
        return 0;
      default:
        break;
      }
    return 1;
  }
  int
  mulib_smtpclient_feed (mulib_smtpclient *c, const void *data, size_t n)
  {
    const unsigned char *p = (const unsigned char *)data;
    if (!c || (!data && n) || c->state < 0 || c->state == 4)
      return 0;
    for (size_t i = 0; i < n; ++i)
      {
        unsigned char ch = p[i];
        if (c->state == 4)
          {
            c->state = -1;
            return 0;
          }
        if (c->cr)
          {
            if (ch != '\n')
              {
                c->state = -1;
                return 0;
              }
            c->cr = 0;
            c->line[c->length] = 0;
            if (!mulib_smtpclient_reply_ (c))
              {
                c->state = -1;
                return 0;
              }
            c->length = 0;
          }
        else if (ch == '\r')
          c->cr = 1;
        else
          {
            if ((ch < 32 && ch != '\t') || ch > 126
                || c->length >= c->capacity - 1)
              {
                c->state = -1;
                return 0;
              }
            c->line[c->length++] = (char)ch;
          }
      }
    return 1;
  }
  int
  mulib_smtpclient_request (mulib_smtpclient *c,
                            mulib_smtpclient_command command, const char *arg)
  {
    static const char *const names[] = { "HELO ", "MAIL FROM:<", "RCPT TO:<",
                                         "DATA",  "RSET",        "NOOP",
                                         "QUIT" };
    size_t n, prefix, suffix;
    if (!c || c->state != 1 || c->length || c->cr || command < MULIB_SMTP_HELO
        || command > MULIB_SMTP_QUIT)
      return 0;
    if (!arg)
      arg = "";
    n = strlen (arg);
    if (command == MULIB_SMTP_HELO)
      {
        if (!n || n > 255)
          return 0;
        for (size_t i = 0; i < n; ++i)
          if ((unsigned char)arg[i] <= 32 || (unsigned char)arg[i] > 126)
            return 0;
      }
    else if (command == MULIB_SMTP_MAIL || command == MULIB_SMTP_RCPT)
      {
        if (!c->greeted
            || (command == MULIB_SMTP_RCPT
                && (!c->mail || !n || c->recipients >= 16))
            || n > 255)
          return 0;
        for (size_t i = 0; i < n; ++i)
          {
            unsigned char ch = (unsigned char)arg[i];
            if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
                  || (ch >= '0' && ch <= '9')
                  || strchr ("!#$%&'*+-/=?^_`{|}~.@", ch)))
              return 0;
          }
      }
    else if (n)
      return 0;
    if (command == MULIB_SMTP_DATA && (!c->mail || !c->recipients))
      return 0;
    prefix = strlen (names[command]);
    suffix = command == MULIB_SMTP_MAIL || command == MULIB_SMTP_RCPT ? 3 : 2;
    if (prefix + suffix > c->capacity || n > c->capacity - prefix - suffix)
      return 0;
    memcpy (c->line, names[command], prefix);
    memcpy (c->line + prefix, arg, n);
    if (suffix == 3)
      c->line[prefix + n++] = '>';
    c->line[prefix + n] = '\r';
    c->line[prefix + n + 1] = '\n';
    /* A new MAIL invalidates the old transaction even if the peer rejects it.
     */
    if (command == MULIB_SMTP_MAIL)
      {
        c->mail = 0;
        c->recipients = 0;
      }
    c->pending = command;
    c->state = 2;
    if (!c->send (c->context, c->line, prefix + n + 2))
      {
        c->state = -1;
        return 0;
      }
    return 1;
  }
  int
  mulib_smtpclient_message (mulib_smtpclient *c, const void *data, size_t n)
  {
    const unsigned char *p = (const unsigned char *)data;
    size_t start = 0;
    if (!c || c->state != 3 || c->length || c->cr
        || !mulib_smtp_message_valid (data, n, c->message_limit,
                                      c->line_limit))
      return 0;
    c->state = 2;
    c->completing = 1;
    for (size_t i = 0; i < n; ++i)
      {
        if (i == start && p[i] == '.' && !c->send (c->context, ".", 1))
          {
            c->state = -1;
            return 0;
          }
        if (p[i] == '\n')
          {
            if (!c->send (c->context, p + start, i - start + 1))
              {
                c->state = -1;
                return 0;
              }
            start = i + 1;
          }
      }
    if (!c->send (c->context, ".\r\n", 3))
      {
        c->state = -1;
        return 0;
      }
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
