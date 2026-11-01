/**
 * @file mulib-dnsnmeserver.h
 * @brief Compatibility spelling for mulib-nameserver, with real forwarding
 * functions. Define MULIB_IMPL_DNSNMESERVER plus MULIB_IMPL_NAMESERVER,
 * MULIB_IMPL_DNSCLIENT and MULIB_IMPL_DNS separately.
 */
#ifndef MULIB_DNSNMESERVER_H
#define MULIB_DNSNMESERVER_H
#include "mulib-nameserver.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef mulib_nameserver_t mulib_dnsnmeserver_t;
  /** @brief Forward to mulib_nameserver_init with identical ownership/errors.
   */
  int mulib_dnsnmeserver_init (mulib_dnsnmeserver_t *server,
                               const char *origin, size_t length,
                               const mulib_dns_rr_t *records, size_t count);
  /** @brief Forward to mulib_nameserver_answer with identical bounds and wire
   * behavior. */
  int mulib_dnsnmeserver_answer (const mulib_dnsnmeserver_t *server,
                                 const unsigned char *query,
                                 size_t query_length, unsigned char *reply,
                                 size_t reply_capacity, size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DNSNMESERVER)                                          \
    && !defined(MULIB_DNSNMESERVER_IMPLEMENTED)
#define MULIB_DNSNMESERVER_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_dnsnmeserver_init (mulib_dnsnmeserver_t *s, const char *origin,
                           size_t n, const mulib_dns_rr_t *records,
                           size_t count)
  {
    return mulib_nameserver_init (s, origin, n, records, count);
  }
  int
  mulib_dnsnmeserver_answer (const mulib_dnsnmeserver_t *s,
                             const unsigned char *q, size_t qn,
                             unsigned char *r, size_t cap, size_t *written)
  {
    return mulib_nameserver_answer (s, q, qn, r, cap, written);
  }
#ifdef __cplusplus
}
#endif
#endif
