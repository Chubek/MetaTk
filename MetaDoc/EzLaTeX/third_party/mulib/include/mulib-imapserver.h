/** @file mulib-imapserver.h
 * @brief Bounded tagged IMAP-inspired session subset; no literals or TLS.
 * Define MULIB_IMPL_IMAPSERVER once. See manual/mulib-imapserver.md.
 */
#ifndef MULIB_IMAPSERVER_H
#define MULIB_IMAPSERVER_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** Encode a NUL-terminated printable ASCII string as an IMAP quoted string,
   * escaping quote/backslash. out/written required. capacity includes final
   * NUL. Returns 0 without changing output/written on invalid input or
   * insufficient space. Source and output must not overlap. Shared with
   * impaclient. */
  int mulib_imap_quote (const char *text, char *out, size_t capacity,
                        size_t *written);
  /** Borrowed callbacks; send all bytes or fail; authorization and mailbox
   * selection return nonzero to accept. select returns an EXISTS count. No
   * reentry. */
  typedef struct mulib_imapserver_io
  {
    int (*send) (void *, const void *, size_t);
    int (*authorize) (void *, const char *, const char *);
    int (*select) (void *, const char *, size_t *);
  } mulib_imapserver_io;
  typedef struct mulib_imapserver
  {
    mulib_imapserver_io io;
    void *context;
    char *line;
    size_t capacity, length, exists;
    int cr, failed, closed, authorized, selected;
  } mulib_imapserver;
  /** Initialize, borrow line[capacity >= 16], emit untagged OK greeting. */
  int mulib_imapserver_init (mulib_imapserver *s, char *line, size_t capacity,
                             const mulib_imapserver_io *io, void *context);
  /** Incrementally consume strict printable-ASCII CRLF lines. NULL only at
   * n=0. Framing/overflow/output failures are terminal (0). Syntax, state and
   * explicitly unsupported commands/literals produce BAD/NO and remain usable
   * (1). */
  int mulib_imapserver_feed (mulib_imapserver *s, const void *data, size_t n);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_IMAPSERVER) && !defined(MULIB_IMAPSERVER_IMPLEMENTED)
#define MULIB_IMAPSERVER_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_imap_quote (const char *text, char *out, size_t capacity,
                    size_t *written)
  {
    size_t need = 2, pos = 0;
    if (!text || !out || !written)
      return 0;
    for (size_t i = 0; text[i]; ++i)
      {
        unsigned char c = (unsigned char)text[i];
        size_t add = c == '"' || c == '\\' ? 2 : 1;
        if (c < 32 || c > 126 || need > SIZE_MAX - add)
          return 0;
        need += add;
      }
    if (need >= capacity)
      return 0;
    out[pos++] = '"';
    for (size_t i = 0; text[i]; ++i)
      {
        if (text[i] == '"' || text[i] == '\\')
          out[pos++] = '\\';
        out[pos++] = text[i];
      }
    out[pos++] = '"';
    out[pos] = 0;
    *written = pos;
    return 1;
  }
  static int
  mulib_imapserver_send_ (mulib_imapserver *s, const char *text)
  {
    if (!s->io.send (s->context, text, strlen (text)))
      {
        s->failed = 1;
        return 0;
      }
    return 1;
  }
  static int
  mulib_imapserver_equal_ (const char *a, const char *b)
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
  mulib_imapserver_tagchar_ (unsigned char c)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
           || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
  }
  static int
  mulib_imapserver_tokens_ (char *line, char *args[4])
  {
    char *r = line;
    int count = 0;
    while (*r)
      {
        char *w;
        while (*r == ' ')
          ++r;
        if (!*r)
          break;
        if (count == 4)
          return -1;
        w = r;
        args[count++] = w;
        if (*r == '"')
          {
            int ended = 0;
            ++r;
            while (*r)
              {
                char c = *r++;
                if (c == '"')
                  {
                    ended = 1;
                    break;
                  }
                if (c == '\\')
                  {
                    c = *r++;
                    if (c != '\\' && c != '"')
                      return -1;
                  }
                *w++ = c;
              }
            if (!ended || (*r && *r != ' '))
              return -1;
          }
        else
          {
            while (*r && *r != ' ')
              {
                if (strchr ("(){%*\"\\]}", *r))
                  return -1;
                *w++ = *r++;
              }
          }
        while (*r == ' ')
          ++r;
        *w = 0;
      }
    return count;
  }
  static int
  mulib_imapserver_tagged_ (mulib_imapserver *s, const char *tag,
                            const char *text)
  {
    char reply[192];
    (void)snprintf (reply, sizeof (reply), "%s %s\r\n", tag, text);
    return mulib_imapserver_send_ (s, reply);
  }
  static int
  mulib_imapserver_command_ (mulib_imapserver *s)
  {
    char tag[64], *args[4];
    size_t k = 0;
    int count;
    while (s->line[k] && s->line[k] != ' ')
      {
        if (k >= sizeof (tag) - 1
            || !mulib_imapserver_tagchar_ ((unsigned char)s->line[k]))
          return mulib_imapserver_send_ (s, "* BAD invalid tag\r\n");
        tag[k] = s->line[k];
        ++k;
      }
    tag[k] = 0;
    if (!k)
      return mulib_imapserver_send_ (s, "* BAD missing tag\r\n");
    {
      size_t command = k;
      while (s->line[command] == ' ')
        ++command;
      if (s->line[command] == '"')
        return mulib_imapserver_tagged_ (s, tag,
                                         "BAD command must be an atom");
    }
    count = mulib_imapserver_tokens_ (s->line, args);
    if (count < 2)
      return mulib_imapserver_tagged_ (s, tag,
                                       "BAD syntax; literals unsupported");
    if (mulib_imapserver_equal_ (args[1], "CAPABILITY") && count == 2)
      {
        if (!mulib_imapserver_send_ (s, "* CAPABILITY X-MULIB-FOCUSED\r\n"))
          return 0;
        return mulib_imapserver_tagged_ (s, tag, "OK capability");
      }
    if (mulib_imapserver_equal_ (args[1], "NOOP") && count == 2)
      return mulib_imapserver_tagged_ (s, tag, "OK noop");
    if (mulib_imapserver_equal_ (args[1], "LOGOUT") && count == 2)
      {
        s->closed = 1;
        if (!mulib_imapserver_send_ (s, "* BYE logout\r\n"))
          return 0;
        return mulib_imapserver_tagged_ (s, tag, "OK logout");
      }
    if (mulib_imapserver_equal_ (args[1], "LOGIN") && count == 4)
      {
        if (s->authorized)
          return mulib_imapserver_tagged_ (s, tag,
                                           "BAD already authenticated");
        if (!s->io.authorize
            || !s->io.authorize (s->context, args[2], args[3]))
          return mulib_imapserver_tagged_ (s, tag, "NO authorization failed");
        s->authorized = 1;
        return mulib_imapserver_tagged_ (s, tag, "OK login");
      }
    if (mulib_imapserver_equal_ (args[1], "SELECT") && count == 3)
      {
        char reply[96];
        size_t exists = 0;
        if (!s->authorized)
          return mulib_imapserver_tagged_ (s, tag, "NO login required");
        s->selected = 0;
        s->exists = 0;
        if (!s->io.select || !s->io.select (s->context, args[2], &exists))
          return mulib_imapserver_tagged_ (s, tag, "NO mailbox unavailable");
        s->selected = 1;
        s->exists = exists;
        (void)snprintf (reply, sizeof (reply), "* %zu EXISTS\r\n", exists);
        if (!mulib_imapserver_send_ (s, reply))
          return 0;
        return mulib_imapserver_tagged_ (s, tag, "OK [READ-ONLY] selected");
      }
    return mulib_imapserver_tagged_ (s, tag,
                                     "BAD unsupported command or arguments");
  }
  int
  mulib_imapserver_init (mulib_imapserver *s, char *line, size_t capacity,
                         const mulib_imapserver_io *io, void *context)
  {
    mulib_imapserver_io callbacks;
    if (!s || !line || capacity < 16 || !io || !io->send)
      return 0;
    callbacks = *io;
    memset (s, 0, sizeof (*s));
    s->line = line;
    s->capacity = capacity;
    s->io = callbacks;
    s->context = context;
    return mulib_imapserver_send_ (s, "* OK MuLIB focused IMAP subset\r\n");
  }
  int
  mulib_imapserver_feed (mulib_imapserver *s, const void *data, size_t n)
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
            s->length = 0;
            if (!mulib_imapserver_command_ (s))
              return 0;
          }
        else if (c == '\r')
          s->cr = 1;
        else
          {
            if (c < 32 || c > 126 || s->length >= s->capacity - 1)
              {
                s->failed = 1;
                return 0;
              }
            s->line[s->length++] = (char)c;
          }
      }
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
