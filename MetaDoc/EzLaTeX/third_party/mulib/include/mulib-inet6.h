/**
 * @file mulib-inet6.h
 * @brief IPv6 literals, canonical hexadecimal formatting and prefix
 * operations. Define MULIB_IMPL_INET6, MULIB_IMPL_INET4 and MULIB_IMPL_PARSEC
 * once.
 */
#ifndef MULIB_INET6_H
#define MULIB_INET6_H
#include "mulib-inet4.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Sixteen address bytes in network order. */
  typedef struct mulib_inet6
  {
    unsigned char bytes[16];
  } mulib_inet6_t;
  /** @brief Parse hex groups, one :: compression and optional final IPv4; no
   * zones/brackets. */
  int mulib_inet6_parse (const char *text, size_t length, mulib_inet6_t *out);
  /** @brief Lowercase hex, longest/first zero run compressed; 40 bytes always
   * suffice. */
  int mulib_inet6_format (const mulib_inet6_t *address, char *out,
                          size_t capacity);
  /** @brief Clear host bits for prefix 0..128; unchanged on invalid arguments.
   */
  int mulib_inet6_network (const mulib_inet6_t *address, unsigned prefix,
                           mulib_inet6_t *out);
  /** @brief Test prefix membership, ignoring host bits in network. */
  int mulib_inet6_contains (const mulib_inet6_t *network, unsigned prefix,
                            const mulib_inet6_t *address);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_INET6) && !defined(MULIB_INET6_IMPLEMENTED)
#define MULIB_INET6_IMPLEMENTED
#include <stdio.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_inet6_parse (const char *s, size_t n, mulib_inet6_t *out)
  {
    unsigned groups[8], count = 0, gap = 9;
    size_t pos = 0;
    mulib_inet6_t value;
    mulib_parsec_parser_t ranges[3], hex, group, parts[2], grammar;
    if (!s || !out || n < 2 || n > 45)
      return 0;
    ranges[0] = mulib_parsec_range ('0', '9');
    ranges[1] = mulib_parsec_range ('a', 'f');
    ranges[2] = mulib_parsec_range ('A', 'F');
    hex = mulib_parsec_choice (ranges, 3);
    group = mulib_parsec_repeat (&hex, 1, 4);
    parts[0] = group;
    parts[1] = mulib_parsec_end ();
    grammar = mulib_parsec_sequence (parts, 2);
    if (s[0] == ':')
      {
        if (s[1] != ':')
          return 0;
        gap = 0;
        pos = 2;
      }
    while (pos < n)
      {
        size_t begin = pos;
        unsigned v = 0;
        mulib_parsec_result_t result;
        if (count == 8)
          return 0;
        while (pos < n && s[pos] != ':')
          ++pos;
        if (memchr (s + begin, '.', pos - begin))
          {
            mulib_inet4_t ipv4;
            if (pos != n || count > 6
                || !mulib_inet4_parse (s + begin, pos - begin, &ipv4))
              return 0;
            groups[count++] = (unsigned)ipv4.bytes[0] * 256 + ipv4.bytes[1];
            groups[count++] = (unsigned)ipv4.bytes[2] * 256 + ipv4.bytes[3];
            break;
          }
        {
          int ok = mulib_parsec_run (&grammar, s + begin, pos - begin, NULL,
                                     &result);
          mulib_parsec_result_free (&result);
          if (ok != 1)
            return 0;
        }
        for (size_t i = begin; i < pos; ++i)
          {
            unsigned char c = (unsigned char)s[i];
            v = v * 16
                + (c <= '9'   ? (unsigned)(c - '0')
                   : c <= 'F' ? (unsigned)(c - 'A' + 10)
                              : (unsigned)(c - 'a' + 10));
          }
        groups[count++] = v;
        if (pos == n)
          break;
        ++pos;
        if (pos < n && s[pos] == ':')
          {
            if (gap != 9)
              return 0;
            gap = count;
            ++pos;
          }
        else if (pos == n)
          return 0;
      }
    if ((gap == 9 && count != 8) || (gap != 9 && count >= 8))
      return 0;
    if (gap != 9)
      {
        unsigned missing = 8 - count;
        for (unsigned i = count; i > gap; --i)
          groups[i - 1 + missing] = groups[i - 1];
        for (unsigned i = 0; i < missing; ++i)
          groups[gap + i] = 0;
      }
    for (unsigned i = 0; i < 8; ++i)
      {
        value.bytes[2 * i] = (unsigned char)(groups[i] >> 8);
        value.bytes[2 * i + 1] = (unsigned char)groups[i];
      }
    *out = value;
    return 1;
  }
  int
  mulib_inet6_format (const mulib_inet6_t *a, char *out, size_t cap)
  {
    unsigned g[8], best = 8, length = 0;
    char text[40];
    size_t pos = 0;
    if (!a || !out)
      return 0;
    for (unsigned i = 0; i < 8; ++i)
      g[i] = (unsigned)a->bytes[2 * i] * 256 + a->bytes[2 * i + 1];
    for (unsigned i = 0; i < 8;)
      {
        unsigned begin = i;
        if (g[i])
          {
            ++i;
            continue;
          }
        while (i < 8 && !g[i])
          ++i;
        if (i - begin > length && i - begin >= 2)
          {
            best = begin;
            length = i - begin;
          }
      }
    for (unsigned i = 0; i < 8;)
      {
        if (i == best)
          {
            text[pos++] = ':';
            text[pos++] = ':';
            i += length;
            continue;
          }
        if (pos && text[pos - 1] != ':')
          text[pos++] = ':';
        pos += (size_t)snprintf (text + pos, sizeof (text) - pos, "%x",
                                 g[i++]);
      }
    text[pos] = 0;
    if (cap <= pos)
      return 0;
    memcpy (out, text, pos + 1);
    return 1;
  }
  int
  mulib_inet6_network (const mulib_inet6_t *a, unsigned prefix,
                       mulib_inet6_t *out)
  {
    mulib_inet6_t value;
    if (!a || !out || prefix > 128)
      return 0;
    for (unsigned i = 0; i < 16; ++i)
      {
        unsigned bits = prefix > 8 ? 8 : prefix;
        value.bytes[i]
            = (unsigned char)(a->bytes[i] & (bits ? 255u << (8 - bits) : 0));
        prefix -= bits;
      }
    *out = value;
    return 1;
  }
  int
  mulib_inet6_contains (const mulib_inet6_t *net, unsigned prefix,
                        const mulib_inet6_t *a)
  {
    mulib_inet6_t x, y;
    return mulib_inet6_network (net, prefix, &x)
           && mulib_inet6_network (a, prefix, &y)
           && !memcmp (x.bytes, y.bytes, 16);
  }
#ifdef __cplusplus
}
#endif
#endif
