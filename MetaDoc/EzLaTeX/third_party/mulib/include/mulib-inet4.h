/**
 * @file mulib-inet4.h
 * @brief Strict IPv4 literals and network-prefix operations; no sockets or
 * DNS. Define MULIB_IMPL_INET4 and MULIB_IMPL_PARSEC in one C11 translation
 * unit.
 */
#ifndef MULIB_INET4_H
#define MULIB_INET4_H
#include "mulib-parsec.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Address bytes in network order; freely copyable. */
  typedef struct mulib_inet4
  {
    unsigned char bytes[4];
  } mulib_inet4_t;
  /** @brief Parse exactly four decimal octets; reject leading zeroes.
   * Unchanged on failure. */
  int mulib_inet4_parse (const char *text, size_t length, mulib_inet4_t *out);
  /** @brief Format into at least 16 bytes; output unchanged if capacity is
   * insufficient. */
  int mulib_inet4_format (const mulib_inet4_t *address, char *out,
                          size_t capacity);
  /** @brief Clear host bits for prefix 0..32. Output may alias address. */
  int mulib_inet4_network (const mulib_inet4_t *address, unsigned prefix,
                           mulib_inet4_t *out);
  /** @brief Test membership ignoring network's host bits; invalid arguments
   * return zero. */
  int mulib_inet4_contains (const mulib_inet4_t *network, unsigned prefix,
                            const mulib_inet4_t *address);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_INET4) && !defined(MULIB_INET4_IMPLEMENTED)
#define MULIB_INET4_IMPLEMENTED
#include <stdio.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_inet4_parse (const char *s, size_t n, mulib_inet4_t *out)
  {
    mulib_parsec_parser_t digit = mulib_parsec_range ('0', '9');
    mulib_parsec_parser_t octet = mulib_parsec_repeat (&digit, 1, 3), parts[8],
                          grammar;
    mulib_parsec_result_t result;
    mulib_inet4_t value;
    int ok;
    if (!s || !out || n < 7 || n > 15)
      return 0;
    for (size_t i = 0; i < 4; ++i)
      {
        parts[2 * i] = mulib_parsec_capture (&octet, i);
        parts[2 * i + 1]
            = i == 3 ? mulib_parsec_end () : mulib_parsec_byte ('.');
      }
    grammar = mulib_parsec_sequence (parts, 8);
    ok = mulib_parsec_run (&grammar, s, n, NULL, &result) == 1;
    if (ok)
      for (size_t i = 0; i < 4; ++i)
        {
          mulib_parsec_capture_t c = result.captures[i];
          unsigned v = 0;
          if (c.length > 1 && s[c.offset] == '0')
            {
              ok = 0;
              break;
            }
          for (size_t j = 0; j < c.length; ++j)
            v = v * 10 + (unsigned)(s[c.offset + j] - '0');
          if (v > 255)
            {
              ok = 0;
              break;
            }
          value.bytes[i] = (unsigned char)v;
        }
    mulib_parsec_result_free (&result);
    if (ok)
      *out = value;
    return ok;
  }
  int
  mulib_inet4_format (const mulib_inet4_t *a, char *out, size_t cap)
  {
    char text[16];
    int n;
    if (!a || !out)
      return 0;
    n = snprintf (text, sizeof (text), "%u.%u.%u.%u", (unsigned)a->bytes[0],
                  (unsigned)a->bytes[1], (unsigned)a->bytes[2],
                  (unsigned)a->bytes[3]);
    if (n < 0 || cap <= (size_t)n)
      return 0;
    memcpy (out, text, (size_t)n + 1);
    return 1;
  }
  int
  mulib_inet4_network (const mulib_inet4_t *a, unsigned prefix,
                       mulib_inet4_t *out)
  {
    mulib_inet4_t value;
    if (!a || !out || prefix > 32)
      return 0;
    for (unsigned i = 0; i < 4; ++i)
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
  mulib_inet4_contains (const mulib_inet4_t *net, unsigned prefix,
                        const mulib_inet4_t *a)
  {
    mulib_inet4_t x, y;
    return mulib_inet4_network (net, prefix, &x)
           && mulib_inet4_network (a, prefix, &y)
           && !memcmp (x.bytes, y.bytes, 4);
  }
#ifdef __cplusplus
}
#endif
#endif
