/**
 * @file mulib-termkey.h
 * @brief Allocation-free incremental UTF-8, VT/xterm key and mouse decoder.
 *
 * Feed bytes and pull events. No terminal I/O or clock is hidden in the API:
 * flush explicitly after an Escape timeout or at end-of-stream. SGR mouse,
 * bracketed paste and basic CSI-u are supported; this is not a full Kitty or
 * terminfo-configurable key protocol implementation. Define MULIB_IMPL_TERMKEY
 * and MULIB_IMPL_UTF8 in one translation unit. See manual/mulib-termkey.md.
 */
#ifndef MULIB_TERMKEY_H
#define MULIB_TERMKEY_H
#include "mulib-utf8.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Fixed decoder/event byte capacity; feed reports backpressure. */
#define MULIB_TERMKEY_CAPACITY 128u
  /** @brief Modifier bits, including xterm modifier-parameter decoding. */
  enum mulib_termkey_modifier
  {
    MULIB_TERMKEY_SHIFT = 1u,
    MULIB_TERMKEY_ALT = 2u,
    MULIB_TERMKEY_CTRL = 4u,
    MULIB_TERMKEY_SUPER = 8u
  };
  /** @brief Text uses CHAR plus a Unicode scalar, not a byte-valued key code.
   */
  typedef enum mulib_termkey_code
  {
    MULIB_TERMKEY_NONE,
    MULIB_TERMKEY_CHAR,
    MULIB_TERMKEY_ESCAPE,
    MULIB_TERMKEY_ENTER,
    MULIB_TERMKEY_TAB,
    MULIB_TERMKEY_BACKSPACE,
    MULIB_TERMKEY_UP,
    MULIB_TERMKEY_DOWN,
    MULIB_TERMKEY_LEFT,
    MULIB_TERMKEY_RIGHT,
    MULIB_TERMKEY_HOME,
    MULIB_TERMKEY_END,
    MULIB_TERMKEY_INSERT,
    MULIB_TERMKEY_DELETE,
    MULIB_TERMKEY_PAGE_UP,
    MULIB_TERMKEY_PAGE_DOWN,
    MULIB_TERMKEY_F1,
    MULIB_TERMKEY_F2,
    MULIB_TERMKEY_F3,
    MULIB_TERMKEY_F4,
    MULIB_TERMKEY_F5,
    MULIB_TERMKEY_F6,
    MULIB_TERMKEY_F7,
    MULIB_TERMKEY_F8,
    MULIB_TERMKEY_F9,
    MULIB_TERMKEY_F10,
    MULIB_TERMKEY_F11,
    MULIB_TERMKEY_F12
  } mulib_termkey_code_t;
  /** @brief Paste payload events carry raw bytes, which may split UTF-8
   * scalars. */
  typedef enum mulib_termkey_event_type
  {
    MULIB_TERMKEY_KEY,
    MULIB_TERMKEY_MOUSE,
    MULIB_TERMKEY_UNKNOWN,
    MULIB_TERMKEY_PASTE_BEGIN,
    MULIB_TERMKEY_PASTE_TEXT,
    MULIB_TERMKEY_PASTE_END
  } mulib_termkey_event_type_t;
  /** @brief Mouse buttons: 1/2/3 left/middle/right, 4..7 wheel
   * up/down/left/right. */
  typedef enum mulib_termkey_mouse_action
  {
    MULIB_TERMKEY_PRESS,
    MULIB_TERMKEY_RELEASE,
    MULIB_TERMKEY_MOVE,
    MULIB_TERMKEY_WHEEL
  } mulib_termkey_mouse_action_t;
  /** @brief Value event; raw bytes are owned inline, coordinates are
   * zero-based. */
  typedef struct mulib_termkey_event
  {
    mulib_termkey_event_type_t type;
    mulib_termkey_code_t key;
    uint32_t codepoint;
    unsigned modifiers, mouse_button;
    mulib_termkey_mouse_action_t mouse_action;
    size_t x, y, length;
    unsigned char raw[MULIB_TERMKEY_CAPACITY];
  } mulib_termkey_event_t;
  /** @brief Decoder state. Initialize/reset with init; no allocation or free
   * needed. */
  typedef struct mulib_termkey
  {
    unsigned char pending[MULIB_TERMKEY_CAPACITY];
    size_t length;
    unsigned paste;
  } mulib_termkey_t;

  /** @brief Reset the decoder, discarding pending bytes and paste mode. */
  void mulib_termkey_init (mulib_termkey_t *decoder);
  /** @brief Copy as many bytes as fit and return that count. Pull events and
   * retry. */
  size_t mulib_termkey_feed (mulib_termkey_t *decoder, const void *bytes,
                             size_t length);
  /**
   * @brief Pull one event: 1 event, 0 incomplete/empty, -1 invalid arguments.
   * flush resolves a lone ESC and incomplete sequences. A full buffer always
   * makes progress. Invalid UTF-8 emits U+FFFD, consuming one bad leading
   * byte.
   */
  int mulib_termkey_next (mulib_termkey_t *decoder,
                          mulib_termkey_event_t *event, int flush);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_TERMKEY_H */

#if defined(MULIB_IMPL_TERMKEY) && !defined(MULIB_TERMKEY_IMPLEMENTED)
#define MULIB_TERMKEY_IMPLEMENTED
#include <limits.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  void
  mulib_termkey_init (mulib_termkey_t *d)
  {
    if (d)
      memset (d, 0, sizeof (*d));
  }
  size_t
  mulib_termkey_feed (mulib_termkey_t *d, const void *bytes, size_t n)
  {
    size_t count;
    if (!d || (!bytes && n) || d->length > MULIB_TERMKEY_CAPACITY)
      return 0;
    count = MULIB_TERMKEY_CAPACITY - d->length;
    if (count > n)
      count = n;
    if (count)
      memmove (d->pending + d->length, bytes, count);
    d->length += count;
    return count;
  }
  static int
  mulib_termkey_take_ (mulib_termkey_t *d, mulib_termkey_event_t *e, size_t n)
  {
    e->length = n;
    memcpy (e->raw, d->pending, n);
    d->length -= n;
    memmove (d->pending, d->pending + n, d->length);
    return 1;
  }
  static int
  mulib_termkey_scalar_ (const unsigned char *p, size_t n, int flush,
                         uint32_t *cp, size_t *used)
  {
    size_t need, i;
    if (!n)
      return 0;
    need = p[0] < 0x80                    ? 1u
           : p[0] >= 0xc2 && p[0] <= 0xdf ? 2u
           : p[0] >= 0xe0 && p[0] <= 0xef ? 3u
           : p[0] >= 0xf0 && p[0] <= 0xf4 ? 4u
                                          : 1u;
    for (i = 1; i < need && i < n; ++i)
      if ((p[i] & 0xc0u) != 0x80u)
        break;
    if (i == n && n < need && !flush)
      return 0;
    if (mulib_utf8_decode (p, n, cp, used))
      return 1;
    *cp = 0xfffdu;
    *used = 1;
    return 1;
  }
  static void
  mulib_termkey_character_ (mulib_termkey_event_t *e, uint32_t cp)
  {
    e->type = MULIB_TERMKEY_KEY;
    e->key = MULIB_TERMKEY_CHAR;
    e->codepoint = cp;
    if (cp == 27)
      e->key = MULIB_TERMKEY_ESCAPE;
    else if (cp == 13 || cp == 10)
      e->key = MULIB_TERMKEY_ENTER;
    else if (cp == 9)
      e->key = MULIB_TERMKEY_TAB;
    else if (cp == 8 || cp == 127)
      e->key = MULIB_TERMKEY_BACKSPACE;
    else if (cp < 32)
      {
        e->modifiers |= MULIB_TERMKEY_CTRL;
        e->codepoint = cp == 0 ? ' ' : cp <= 26 ? 'a' + cp - 1 : cp + 64;
      }
  }
  static int
  mulib_termkey_params_ (const unsigned char *p, size_t n, unsigned values[4],
                         size_t *count)
  {
    size_t i, k = 0;
    unsigned value = 0;
    if (!n)
      {
        *count = 0;
        return 1;
      }
    for (i = 0; i < n; ++i)
      {
        if (p[i] >= '0' && p[i] <= '9')
          {
            if (value > (0x10ffffu - (p[i] - '0')) / 10u)
              return 0;
            value = value * 10u + p[i] - '0';
          }
        else if (p[i] == ';')
          {
            if (k == 3)
              return 0;
            values[k++] = value;
            value = 0;
          }
        else
          return 0;
      }
    values[k++] = value;
    *count = k;
    return 1;
  }
  static int
  mulib_termkey_modifier_ (unsigned parameter, unsigned *out)
  {
    if (parameter < 1 || parameter > 16)
      return 0;
    *out = parameter - 1;
    return 1;
  }
  static int
  mulib_termkey_sequence_ (mulib_termkey_t *d, mulib_termkey_event_t *e,
                           size_t n)
  {
    const unsigned char *p = d->pending + 2;
    size_t size = n - 3, count = 0;
    unsigned v[4] = { 0 };
    unsigned final = d->pending[n - 1];
    int mouse = size && *p == '<';
    e->type = MULIB_TERMKEY_UNKNOWN;
    if (mouse)
      {
        ++p;
        --size;
      }
    if (!mulib_termkey_params_ (p, size, v, &count))
      return mulib_termkey_take_ (d, e, n);
    if (mouse)
      {
        unsigned button;
        if (d->pending[1] != '[' || count != 3
            || (final != 'M' && final != 'm') || v[0] > 127 || !v[1] || !v[2])
          return mulib_termkey_take_ (d, e, n);
        e->type = MULIB_TERMKEY_MOUSE;
        e->x = v[1] - 1u;
        e->y = v[2] - 1u;
        e->modifiers = ((v[0] & 4u) ? MULIB_TERMKEY_SHIFT : 0u)
                       | ((v[0] & 8u) ? MULIB_TERMKEY_ALT : 0u)
                       | ((v[0] & 16u) ? MULIB_TERMKEY_CTRL : 0u);
        button = v[0] & 3u;
        e->mouse_button = (v[0] & 64u)  ? button + 4u
                          : button == 3 ? 0u
                                        : button + 1u;
        e->mouse_action = (v[0] & 64u)   ? MULIB_TERMKEY_WHEEL
                          : final == 'm' ? MULIB_TERMKEY_RELEASE
                          : (v[0] & 32u) ? MULIB_TERMKEY_MOVE
                                         : MULIB_TERMKEY_PRESS;
        return mulib_termkey_take_ (d, e, n);
      }
    if (d->pending[1] == '[' && final == '~' && count == 1
        && (v[0] == 200 || v[0] == 201))
      {
        e->type = v[0] == 200 ? MULIB_TERMKEY_PASTE_BEGIN
                              : MULIB_TERMKEY_PASTE_END;
        d->paste = v[0] == 200;
        return mulib_termkey_take_ (d, e, n);
      }
    if (d->pending[1] == '[' && final == 'u' && count >= 1 && count <= 2
        && v[0] <= 0x10ffffu && !(v[0] >= 0xd800u && v[0] <= 0xdfffu))
      {
        if (count == 2 && !mulib_termkey_modifier_ (v[1], &e->modifiers))
          return mulib_termkey_take_ (d, e, n);
        mulib_termkey_character_ (e, v[0]);
        return mulib_termkey_take_ (d, e, n);
      }
    if (count > 2
        || (count == 2 && !mulib_termkey_modifier_ (v[1], &e->modifiers)))
      return mulib_termkey_take_ (d, e, n);
    if (final != '~' && count && v[0] != 0 && v[0] != 1)
      return mulib_termkey_take_ (d, e, n);
    switch (final)
      {
      case 'A':
        e->key = MULIB_TERMKEY_UP;
        break;
      case 'B':
        e->key = MULIB_TERMKEY_DOWN;
        break;
      case 'C':
        e->key = MULIB_TERMKEY_RIGHT;
        break;
      case 'D':
        e->key = MULIB_TERMKEY_LEFT;
        break;
      case 'H':
        e->key = MULIB_TERMKEY_HOME;
        break;
      case 'F':
        e->key = MULIB_TERMKEY_END;
        break;
      case 'P':
        e->key = MULIB_TERMKEY_F1;
        break;
      case 'Q':
        e->key = MULIB_TERMKEY_F2;
        break;
      case 'R':
        e->key = MULIB_TERMKEY_F3;
        break;
      case 'S':
        e->key = MULIB_TERMKEY_F4;
        break;
      case 'Z':
        e->key = MULIB_TERMKEY_TAB;
        e->modifiers |= MULIB_TERMKEY_SHIFT;
        break;
      case '~':
        if (!count || d->pending[1] != '[')
          break;
        switch (v[0])
          {
          case 1:
          case 7:
            e->key = MULIB_TERMKEY_HOME;
            break;
          case 2:
            e->key = MULIB_TERMKEY_INSERT;
            break;
          case 3:
            e->key = MULIB_TERMKEY_DELETE;
            break;
          case 4:
          case 8:
            e->key = MULIB_TERMKEY_END;
            break;
          case 5:
            e->key = MULIB_TERMKEY_PAGE_UP;
            break;
          case 6:
            e->key = MULIB_TERMKEY_PAGE_DOWN;
            break;
          case 11:
            e->key = MULIB_TERMKEY_F1;
            break;
          case 12:
            e->key = MULIB_TERMKEY_F2;
            break;
          case 13:
            e->key = MULIB_TERMKEY_F3;
            break;
          case 14:
            e->key = MULIB_TERMKEY_F4;
            break;
          case 15:
            e->key = MULIB_TERMKEY_F5;
            break;
          case 17:
            e->key = MULIB_TERMKEY_F6;
            break;
          case 18:
            e->key = MULIB_TERMKEY_F7;
            break;
          case 19:
            e->key = MULIB_TERMKEY_F8;
            break;
          case 20:
            e->key = MULIB_TERMKEY_F9;
            break;
          case 21:
            e->key = MULIB_TERMKEY_F10;
            break;
          case 23:
            e->key = MULIB_TERMKEY_F11;
            break;
          case 24:
            e->key = MULIB_TERMKEY_F12;
            break;
          }
        break;
      }
    if (e->key != MULIB_TERMKEY_NONE)
      e->type = MULIB_TERMKEY_KEY;
    return mulib_termkey_take_ (d, e, n);
  }

  int
  mulib_termkey_next (mulib_termkey_t *d, mulib_termkey_event_t *e, int flush)
  {
    static const unsigned char end[] = "\033[201~";
    size_t n, i, used;
    uint32_t cp;
    if (!d || !e || d->length > MULIB_TERMKEY_CAPACITY)
      return -1;
    if (!d->length)
      return 0;
    memset (e, 0, sizeof (*e));
    n = d->length;
    if (d->paste)
      {
        for (i = 0; i + 6 <= n; ++i)
          {
            if (!memcmp (d->pending + i, end, 6))
              {
                if (!i)
                  {
                    d->paste = 0;
                    e->type = MULIB_TERMKEY_PASTE_END;
                    return mulib_termkey_take_ (d, e, 6);
                  }
                e->type = MULIB_TERMKEY_PASTE_TEXT;
                return mulib_termkey_take_ (d, e, i);
              }
          }
        used = n;
        if (!flush)
          {
            for (i = n < 5 ? n : 5; i; --i)
              if (!memcmp (d->pending + n - i, end, i))
                {
                  used = n - i;
                  break;
                }
          }
        if (!used)
          return 0;
        e->type = MULIB_TERMKEY_PASTE_TEXT;
        return mulib_termkey_take_ (d, e, used);
      }
    if (d->pending[0] != 27)
      {
        if (!mulib_termkey_scalar_ (d->pending, n, flush, &cp, &used))
          return 0;
        mulib_termkey_character_ (e, cp);
        return mulib_termkey_take_ (d, e, used);
      }
    if (n == 1)
      {
        if (!flush)
          return 0;
        mulib_termkey_character_ (e, 27);
        return mulib_termkey_take_ (d, e, 1);
      }
    if (d->pending[1] == '[' || d->pending[1] == 'O')
      {
        for (i = 2; i < n; ++i)
          {
            if (d->pending[i] >= 0x40 && d->pending[i] <= 0x7e)
              return mulib_termkey_sequence_ (d, e, i + 1);
            if (d->pending[i] < 0x20 || d->pending[i] > 0x3f)
              {
                e->type = MULIB_TERMKEY_UNKNOWN;
                return mulib_termkey_take_ (d, e, i);
              }
          }
        if (!flush && n < MULIB_TERMKEY_CAPACITY)
          return 0;
        e->type = MULIB_TERMKEY_UNKNOWN;
        return mulib_termkey_take_ (d, e, n);
      }
    if (d->pending[1] == 27)
      {
        mulib_termkey_character_ (e, 27);
        return mulib_termkey_take_ (d, e, 1);
      }
    if (!mulib_termkey_scalar_ (d->pending + 1, n - 1, flush, &cp, &used))
      return 0;
    e->modifiers = MULIB_TERMKEY_ALT;
    mulib_termkey_character_ (e, cp);
    return mulib_termkey_take_ (d, e, used + 1);
  }
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TERMKEY_IMPLEMENTED */
