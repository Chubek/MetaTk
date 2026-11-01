/**
 * @file mulib-inte6.h
 * @brief Historical IPv6 spelling; includes the canonical mulib-inet6.h.
 * Existing MULIB_IMPL_INET6 users work unchanged. MULIB_IMPL_INTE6 implements
 * only the mulib_inte6_* forwarding functions, not its dependencies.
 */
/* Keep re-inclusion with a newly defined historical INET6 switch working. */
#include "mulib-inet6.h"
#ifndef MULIB_INTE6_H
#define MULIB_INTE6_H
#ifdef __cplusplus
extern "C"
{
#endif
  typedef mulib_inet6_t mulib_inte6_t;
  /** @brief Forward to mulib_inet6_parse; same ownership and return
   * convention. */
  int mulib_inte6_parse (const char *text, size_t length, mulib_inte6_t *out);
  /** @brief Forward to mulib_inet6_format. */
  int mulib_inte6_format (const mulib_inte6_t *address, char *out,
                          size_t capacity);
  /** @brief Forward to mulib_inet6_network. */
  int mulib_inte6_network (const mulib_inte6_t *address, unsigned prefix,
                           mulib_inte6_t *out);
  /** @brief Forward to mulib_inet6_contains. */
  int mulib_inte6_contains (const mulib_inte6_t *network, unsigned prefix,
                            const mulib_inte6_t *address);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_INTE6) && !defined(MULIB_INTE6_IMPLEMENTED)
#define MULIB_INTE6_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_inte6_parse (const char *s, size_t n, mulib_inte6_t *out)
  {
    return mulib_inet6_parse (s, n, out);
  }
  int
  mulib_inte6_format (const mulib_inte6_t *a, char *out, size_t n)
  {
    return mulib_inet6_format (a, out, n);
  }
  int
  mulib_inte6_network (const mulib_inte6_t *a, unsigned p, mulib_inte6_t *out)
  {
    return mulib_inet6_network (a, p, out);
  }
  int
  mulib_inte6_contains (const mulib_inte6_t *a, unsigned p,
                        const mulib_inte6_t *b)
  {
    return mulib_inet6_contains (a, p, b);
  }
#ifdef __cplusplus
}
#endif
#endif
