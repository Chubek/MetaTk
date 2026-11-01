/**
 * @file mulib-dnsresolver.h
 * @brief Callback-backed A/AAAA resolution with an optional positive TTL
 * cache. Define MULIB_IMPL_DNSRESOLVER, MULIB_IMPL_DNSCLIENT,
 * MULIB_IMPL_DNSCACHE, and MULIB_IMPL_DNS separately. No sockets or hidden
 * dependency switches.
 */
#ifndef MULIB_DNSRESOLVER_H
#define MULIB_DNSRESOLVER_H
#include "mulib-dnscache.h"
#include "mulib-dnsclient.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Borrows callback state/cache. Caller serializes calls and supplies
   * unpredictable IDs if needed. */
  typedef struct mulib_dnsresolver
  {
    mulib_dnsclient_transport_fn transport;
    void *user;
    mulib_dnscache_t *cache;
    uint16_t next_id;
  } mulib_dnsresolver_t;
  /** @brief Initialize a resolver; cache may be NULL. first_id is
   * application-selected. */
  int mulib_dnsresolver_init (mulib_dnsresolver_t *resolver,
                              mulib_dnsclient_transport_fn transport,
                              void *user, mulib_dnscache_t *cache,
                              uint16_t first_id);
  /**
   * @brief Resolve one IN A/AAAA RRset. All buffers are borrowed; result is a
   * value copy. Query/reply workspaces must not overlap any other
   * argument/storage. On a cache hit they are unused. No alias chasing,
   * retries, negative caching or partial sets.
   */
  int mulib_dnsresolver_resolve (mulib_dnsresolver_t *resolver,
                                 const char *name, size_t length,
                                 uint16_t type, uint64_t now,
                                 unsigned char *query, size_t query_capacity,
                                 unsigned char *reply, size_t reply_capacity,
                                 mulib_dnscache_result_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DNSRESOLVER) && !defined(MULIB_DNSRESOLVER_IMPLEMENTED)
#define MULIB_DNSRESOLVER_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_dnsresolver_init (mulib_dnsresolver_t *r,
                          mulib_dnsclient_transport_fn fn, void *user,
                          mulib_dnscache_t *cache, uint16_t first_id)
  {
    if (!r || !fn || (cache && cache->capacity && !cache->entries))
      return MULIB_DNS_INVALID;
    r->transport = fn;
    r->user = user;
    r->cache = cache;
    r->next_id = first_id;
    return MULIB_DNS_OK;
  }
  int
  mulib_dnsresolver_resolve (mulib_dnsresolver_t *r, const char *name,
                             size_t n, uint16_t type, uint64_t now,
                             unsigned char *query, size_t qcap,
                             unsigned char *reply, size_t rcap,
                             mulib_dnscache_result_t *out)
  {
    char key[256];
    mulib_dnscache_result_t result;
    mulib_dns_message_t m;
    mulib_dns_iterator_t it;
    mulib_dns_question_t question;
    mulib_dns_rr_t rr;
    size_t qn, rn;
    unsigned sec;
    int rc;
    if (!r || !r->transport || !out)
      return MULIB_DNS_INVALID;
    if (type != MULIB_DNS_A && type != MULIB_DNS_AAAA)
      return MULIB_DNS_UNSUPPORTED;
    rc = mulib_dns_name_normalize (name, n, key);
    if (rc != MULIB_DNS_OK)
      return rc;
    if (r->cache)
      {
        rc = mulib_dnscache_get (r->cache, key, strlen (key), type, now, out);
        if (rc != MULIB_DNS_NOTFOUND)
          return rc;
      }
    if (!query || !reply)
      return MULIB_DNS_INVALID;
    rc = mulib_dnsclient_query (key, strlen (key), type, r->next_id, 1, query,
                                qcap, &qn);
    if (rc != MULIB_DNS_OK)
      return rc;
    r->next_id = (uint16_t)(r->next_id + 1u);
    rc = mulib_dnsclient_exchange (r->transport, r->user, query, qn, reply,
                                   rcap, &rn, &m);
    if (rc != MULIB_DNS_OK)
      return rc;
    (void)rn;
    memset (&result, 0, sizeof (result));
    result.type = type;
    result.ttl = UINT32_MAX;
    mulib_dns_iterator_init (&it, &m);
    rc = mulib_dns_next_question (&it, &question);
    if (rc != MULIB_DNS_OK)
      return rc;
    while ((rc = mulib_dns_next_rr (&it, &rr, &sec)) == MULIB_DNS_OK)
      {
        uint32_t ttl;
        if (sec != 1 || rr.class_ != MULIB_DNS_IN
            || !mulib_dns_name_equal (rr.name, key))
          continue;
        if (rr.type == MULIB_DNS_CNAME)
          return MULIB_DNS_UNSUPPORTED;
        if (rr.type != type)
          continue;
        if (result.count == 8)
          return MULIB_DNS_LIMIT;
        memcpy (result.addresses[result.count++], rr.data, rr.data_length);
        ttl = rr.ttl > INT32_MAX ? 0 : rr.ttl;
        if (ttl < result.ttl)
          result.ttl = ttl;
      }
    if (rc != MULIB_DNS_END)
      return rc;
    if (!result.count)
      return MULIB_DNS_NOTFOUND;
    if (r->cache)
      {
        rc = mulib_dnscache_put (r->cache, key, strlen (key), &result, now);
        if (rc != MULIB_DNS_OK && rc != MULIB_DNS_LIMIT)
          return rc;
      }
    *out = result;
    return MULIB_DNS_OK;
  }
#ifdef __cplusplus
}
#endif
#endif
