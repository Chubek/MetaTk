/** @file mulib-impaclient.h
 * @brief Focused tagged IMAP client. The historical spelling "impaclient" is
 * intentional; public symbols and switch preserve it. Depends on imapserver's
 * quote helper. Define MULIB_IMPL_IMAPSERVER and MULIB_IMPL_IMPACLIENT once.
 */
#ifndef MULIB_IMPACLIENT_H
#define MULIB_IMPACLIENT_H
#include "mulib-imapserver.h"
#include <limits.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_impaclient_command
  {
    MULIB_IMAP_CAPABILITY,
    MULIB_IMAP_LOGIN,
    MULIB_IMAP_SELECT,
    MULIB_IMAP_NOOP,
    MULIB_IMAP_LOGOUT
  } mulib_impaclient_command;
  /** state: 0 greeting, 1 ready, 2 waiting, 3 closed, -1 failed. ok reports
   * last tagged OK (1), NO/BAD (0). exists is last SELECT's bounded size_t
   * count. */
  typedef struct mulib_impaclient
  {
    int (*send) (void *, const void *, size_t);
    void *context;
    char *line;
    size_t capacity, length, exists;
    unsigned long serial;
    char tag[32];
    int cr, state, ok, authorized, selected, bye, capability;
    mulib_impaclient_command pending;
  } mulib_impaclient;
  /** Borrow line[capacity >= 16]; send is all-or-fail and may not reenter. */
  int mulib_impaclient_init (mulib_impaclient *c, char *line, size_t capacity,
                             int (*send) (void *, const void *, size_t),
                             void *context);
  /** Incremental strict-CRLF reply parser for the documented subset. Unknown
   * untagged responses, continuation/literal responses, mismatched tags, count
   * overflow and framing/line overflow fail terminally. NULL only at zero n.
   */
  int mulib_impaclient_feed (mulib_impaclient *c, const void *data, size_t n);
  /** Queue one auto-tagged command. LOGIN uses a,b; SELECT uses a; all other
   * arguments must be NULL/empty. String arguments always use safe quoting.
   * State, capacity, argument and tag-exhaustion failures send nothing. */
  int mulib_impaclient_request (mulib_impaclient *c,
                                mulib_impaclient_command command,
                                const char *a, const char *b);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_IMPACLIENT) && !defined(MULIB_IMPACLIENT_IMPLEMENTED)
#define MULIB_IMPACLIENT_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_impaclient_init (mulib_impaclient *c, char *line, size_t capacity,
                         int (*send) (void *, const void *, size_t),
                         void *context)
  {
    if (!c || !line || capacity < 16 || !send)
      return 0;
    memset (c, 0, sizeof (*c));
    c->line = line;
    c->capacity = capacity;
    c->send = send;
    c->context = context;
    return 1;
  }
  static int
  mulib_impaclient_reply_ (mulib_impaclient *c)
  {
    char *p = c->line;
    size_t taglen = strlen (c->tag);
    if (c->state == 0)
      {
        if (strncmp (p, "* OK ", 5) != 0)
          return 0;
        c->state = 1;
        return 1;
      }
    if (c->state != 2)
      return 0;
    if (strncmp (p, "* CAPABILITY ", 13) == 0)
      {
        if (c->pending != MULIB_IMAP_CAPABILITY
            || strcmp (p + 13, "X-MULIB-FOCUSED") != 0)
          return 0;
        c->capability = 1;
        return 1;
      }
    if (strncmp (p, "* BYE ", 6) == 0)
      {
        if (c->pending != MULIB_IMAP_LOGOUT)
          return 0;
        c->bye = 1;
        return 1;
      }
    if (p[0] == '*' && p[1] == ' ' && p[2] >= '0' && p[2] <= '9')
      {
        size_t value = 0, i = 2;
        if (c->pending != MULIB_IMAP_SELECT)
          return 0;
        while (p[i] >= '0' && p[i] <= '9')
          {
            size_t digit = (size_t)(p[i++] - '0');
            if (value > (SIZE_MAX - digit) / 10)
              return 0;
            value = value * 10 + digit;
          }
        if (strcmp (p + i, " EXISTS") != 0)
          return 0;
        c->exists = value;
        return 1;
      }
    if (c->length < taglen + 3 || strncmp (p, c->tag, taglen) != 0
        || p[taglen] != ' ')
      return 0;
    p += taglen + 1;
    if (strncmp (p, "OK ", 3) == 0 || strcmp (p, "OK") == 0)
      c->ok = 1;
    else if (strncmp (p, "NO ", 3) == 0 || strcmp (p, "NO") == 0
             || strncmp (p, "BAD ", 4) == 0 || strcmp (p, "BAD") == 0)
      c->ok = 0;
    else
      return 0;
    c->state = 1;
    if (c->ok)
      {
        if (c->pending == MULIB_IMAP_LOGIN)
          c->authorized = 1;
        if (c->pending == MULIB_IMAP_SELECT)
          c->selected = 1;
        if (c->pending == MULIB_IMAP_LOGOUT)
          {
            if (!c->bye)
              return 0;
            c->state = 3;
          }
      }
    return 1;
  }
  int
  mulib_impaclient_feed (mulib_impaclient *c, const void *data, size_t n)
  {
    const unsigned char *p = (const unsigned char *)data;
    if (!c || (!data && n) || c->state < 0 || c->state == 3)
      return 0;
    for (size_t i = 0; i < n; ++i)
      {
        unsigned char ch = p[i];
        if (c->state == 3)
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
            if (!mulib_impaclient_reply_ (c))
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
  mulib_impaclient_request (mulib_impaclient *c,
                            mulib_impaclient_command command, const char *a,
                            const char *b)
  {
    static const char *const names[]
        = { "CAPABILITY", "LOGIN", "SELECT", "NOOP", "LOGOUT" };
    char tag[32];
    size_t pos, len, used;
    int z;
    if (!c || c->state != 1 || c->length || c->cr || c->serial == ULONG_MAX
        || command < MULIB_IMAP_CAPABILITY || command > MULIB_IMAP_LOGOUT)
      return 0;
    if (!a)
      a = "";
    if (!b)
      b = "";
    if (command == MULIB_IMAP_LOGIN)
      {
        if (c->authorized || !*a)
          return 0;
      }
    else if (command == MULIB_IMAP_SELECT)
      {
        if (!c->authorized || !*a || *b)
          return 0;
      }
    else if (*a || *b)
      return 0;
    z = snprintf (tag, sizeof (tag), "T%lu", c->serial + 1);
    if (z < 0 || (size_t)z >= sizeof (tag))
      return 0;
    pos = (size_t)z;
    len = strlen (names[command]);
    if (pos + len + 4 > c->capacity)
      return 0;
    memcpy (c->line, tag, pos);
    c->line[pos++] = ' ';
    memcpy (c->line + pos, names[command], len);
    pos += len;
    if (command == MULIB_IMAP_LOGIN || command == MULIB_IMAP_SELECT)
      {
        c->line[pos++] = ' ';
        if (!mulib_imap_quote (a, c->line + pos, c->capacity - pos, &used))
          return 0;
        pos += used;
        if (command == MULIB_IMAP_LOGIN)
          {
            if (pos >= c->capacity - 1)
              return 0;
            c->line[pos++] = ' ';
            if (!mulib_imap_quote (b, c->line + pos, c->capacity - pos, &used))
              return 0;
            pos += used;
          }
      }
    if (c->capacity - pos < 2)
      return 0;
    c->line[pos++] = '\r';
    c->line[pos++] = '\n';
    strcpy (c->tag, tag);
    ++c->serial;
    c->pending = command;
    c->state = 2;
    c->ok = 0;
    c->bye = 0;
    if (command == MULIB_IMAP_SELECT)
      {
        c->selected = 0;
        c->exists = 0;
      }
    if (command == MULIB_IMAP_CAPABILITY)
      c->capability = 0;
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
