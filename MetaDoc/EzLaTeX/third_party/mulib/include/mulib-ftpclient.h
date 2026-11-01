/** @file mulib-ftpclient.h
 * @brief Incremental FTP reply/session client; data transport is
 * application-owned. Define MULIB_IMPL_FTPCLIENT once; see
 * manual/mulib-ftpclient.md.
 */
#ifndef MULIB_FTPCLIENT_H
#define MULIB_FTPCLIENT_H
#include <stddef.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** Supported control commands. TYPE accepts only I. */
  typedef enum mulib_ftpclient_command
  {
    MULIB_FTP_USER,
    MULIB_FTP_PASS,
    MULIB_FTP_NOOP,
    MULIB_FTP_PWD,
    MULIB_FTP_CWD,
    MULIB_FTP_TYPE,
    MULIB_FTP_RETR,
    MULIB_FTP_STOR,
    MULIB_FTP_DELE,
    MULIB_FTP_QUIT
  } mulib_ftpclient_command;
  /** State: 0 greeting, 1 ready, 2 waiting, 3 transfer, 4 closed, -1 failed.
   * code is last complete numeric reply. Buffer and context are borrowed. */
  typedef struct mulib_ftpclient
  {
    int (*send) (void *, const void *, size_t);
    void *context;
    char *line;
    size_t capacity, length;
    int cr, state, code, multiline;
    mulib_ftpclient_command pending;
  } mulib_ftpclient;
  /** Initialize without I/O; line capacity >= 8; send consumes all bytes or
   * fails. Callbacks may not reenter either side of a connected session. */
  int mulib_ftpclient_init (mulib_ftpclient *c, char *line, size_t capacity,
                            int (*send) (void *, const void *, size_t),
                            void *context);
  /** Consume arbitrary chunks of strict CRLF replies, including multiline
   * replies. NULL data only at n=0. Malformed/oversized/unexpected replies are
   * terminal. */
  int mulib_ftpclient_feed (mulib_ftpclient *c, const void *data, size_t n);
  /** Submit one command when ready. arg may be NULL for no-argument commands
   * or PASS (empty password). Rejects controls, high bytes, wrong args,
   * state/capacity errors without sending. Output failure is terminal. Return
   * 1 on success. */
  int mulib_ftpclient_request (mulib_ftpclient *c,
                               mulib_ftpclient_command command,
                               const char *arg);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_FTPCLIENT) && !defined(MULIB_FTPCLIENT_IMPLEMENTED)
#define MULIB_FTPCLIENT_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_ftpclient_init (mulib_ftpclient *c, char *line, size_t capacity,
                        int (*send) (void *, const void *, size_t),
                        void *context)
  {
    if (!c || !line || capacity < 8 || !send)
      return 0;
    memset (c, 0, sizeof (*c));
    c->line = line;
    c->capacity = capacity;
    c->send = send;
    c->context = context;
    return 1;
  }
  static int
  mulib_ftpclient_reply_ (mulib_ftpclient *c)
  {
    char *p = c->line;
    int code = 0, numeric = c->length >= 4 && p[0] >= '1' && p[0] <= '5'
                            && p[1] >= '0' && p[1] <= '9' && p[2] >= '0'
                            && p[2] <= '9';
    if (c->state != 0 && c->state != 2 && c->state != 3)
      return 0;
    if (numeric)
      code = (p[0] - '0') * 100 + (p[1] - '0') * 10 + p[2] - '0';
    if (c->multiline)
      {
        if (!numeric || code != c->multiline || p[3] != ' ')
          return 1;
        c->multiline = 0;
      }
    else
      {
        if (!numeric || (p[3] != ' ' && p[3] != '-'))
          return 0;
        if (p[3] == '-')
          {
            c->multiline = code;
            return 1;
          }
      }
    c->code = code;
    if (c->state == 0)
      {
        if (code != 220)
          return 0;
        c->state = 1;
      }
    else if (c->state == 2)
      {
        if (code == 125 || code == 150)
          {
            if (c->pending != MULIB_FTP_RETR && c->pending != MULIB_FTP_STOR)
              return 0;
            c->state = 3;
          }
        else if (code >= 400)
          c->state = code == 421 ? 4 : 1;
        else
          {
            int expected = 0;
            switch (c->pending)
              {
              case MULIB_FTP_USER:
                expected = code == 230 || code == 331;
                break;
              case MULIB_FTP_PASS:
                expected = code == 230;
                break;
              case MULIB_FTP_PWD:
                expected = code == 257;
                break;
              case MULIB_FTP_CWD:
              case MULIB_FTP_DELE:
                expected = code == 250;
                break;
              case MULIB_FTP_TYPE:
              case MULIB_FTP_NOOP:
                expected = code == 200;
                break;
              case MULIB_FTP_QUIT:
                expected = code == 221;
                break;
              default:
                break;
              }
            if (!expected)
              return 0;
            c->state = code == 221 ? 4 : 1;
          }
      }
    else if (c->state == 3)
      {
        if (code != 226 && code != 250 && code < 400)
          return 0;
        c->state = code == 421 ? 4 : 1;
      }
    else
      return 0;
    return 1;
  }
  int
  mulib_ftpclient_feed (mulib_ftpclient *c, const void *data, size_t n)
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
            if (!mulib_ftpclient_reply_ (c))
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
            if (ch < 32 || ch > 126 || c->length >= c->capacity - 1)
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
  mulib_ftpclient_request (mulib_ftpclient *c, mulib_ftpclient_command command,
                           const char *arg)
  {
    static const char *const names[]
        = { "USER", "PASS", "NOOP", "PWD",  "CWD",
            "TYPE", "RETR", "STOR", "DELE", "QUIT" };
    size_t n, v, pos;
    int hasarg;
    if (!c || c->state != 1 || c->length || c->cr || command < MULIB_FTP_USER
        || command > MULIB_FTP_QUIT)
      return 0;
    if (!arg)
      arg = "";
    n = strlen (arg);
    v = strlen (names[command]);
    hasarg = command != MULIB_FTP_NOOP && command != MULIB_FTP_PWD
             && command != MULIB_FTP_QUIT;
    if ((!hasarg && n) || (hasarg && !n && command != MULIB_FTP_PASS)
        || (command == MULIB_FTP_TYPE && strcmp (arg, "I") != 0))
      return 0;
    for (size_t i = 0; i < n; ++i)
      if ((unsigned char)arg[i] < 32 || (unsigned char)arg[i] > 126)
        return 0;
    if (v + 2 + (size_t)hasarg > c->capacity
        || n > c->capacity - v - 2 - (size_t)hasarg)
      return 0;
    memcpy (c->line, names[command], v);
    pos = v;
    if (hasarg)
      {
        c->line[pos++] = ' ';
        memcpy (c->line + pos, arg, n);
        pos += n;
      }
    c->line[pos++] = '\r';
    c->line[pos++] = '\n';
    c->pending = command;
    c->state = 2;
    if (!c->send (c->context, c->line, pos))
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
