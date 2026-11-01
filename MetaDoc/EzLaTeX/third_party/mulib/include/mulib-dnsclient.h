/**
 * @file mulib-dnsclient.h
 * @brief Single-question DNS transactions over an application transport
 * callback. Define MULIB_IMPL_DNSCLIENT and, separately, MULIB_IMPL_DNS.
 */
#ifndef MULIB_DNSCLIENT_H
#define MULIB_DNSCLIENT_H
#include "mulib-dns.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /**
   * @brief Return MULIB_DNS_OK and set reply_length, or a negative error.
   * Buffers are borrowed only for this call. The callback implements peer
   * checking, timeout, framing and I/O; it must never write beyond
   * reply_capacity.
   */
  typedef int (*mulib_dnsclient_transport_fn) (
      void *user, const unsigned char *query, size_t query_length,
      unsigned char *reply, size_t reply_capacity, size_t *reply_length);
  /** @brief Build one IN question, using only RD when recursion is nonzero. */
  int mulib_dnsclient_query (const char *name, size_t length, uint16_t type,
                             uint16_t id, int recursion, unsigned char *packet,
                             size_t capacity, size_t *written);
  /** @brief Require one IN standard question, no RRs and no flags except RD.
   */
  int mulib_dnsclient_validate_query (const mulib_dns_message_t *message,
                                      mulib_dns_question_t *question);
  /** @brief Check ID/question/RD/opcode/flags, reject TC and map RCODE 3 to
   * NOTFOUND. */
  int mulib_dnsclient_validate_reply (const mulib_dns_message_t *query,
                                      const mulib_dns_message_t *reply);
  /** @brief Invoke callback, parse/validate reply; publish borrowed reply view
   * only on success. Query/reply may overlap if the transport supports it;
   * expected question is snapshotted. */
  int mulib_dnsclient_exchange (mulib_dnsclient_transport_fn transport,
                                void *user, const unsigned char *query,
                                size_t query_length, unsigned char *reply,
                                size_t reply_capacity, size_t *reply_length,
                                mulib_dns_message_t *message);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DNSCLIENT) && !defined(MULIB_DNSCLIENT_IMPLEMENTED)
#define MULIB_DNSCLIENT_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_dnsclient_type_ (uint16_t type)
  {
    return type == MULIB_DNS_A || type == MULIB_DNS_AAAA
           || type == MULIB_DNS_NS || type == MULIB_DNS_CNAME
           || type == MULIB_DNS_PTR || type == MULIB_DNS_MX
           || type == MULIB_DNS_TXT || type == MULIB_DNS_ANY;
  }
  int
  mulib_dnsclient_query (const char *name, size_t n, uint16_t type,
                         uint16_t id, int recursion, unsigned char *packet,
                         size_t cap, size_t *written)
  {
    mulib_dns_question_t q;
    mulib_dns_writer_t w;
    int rc;
    if (!packet || !written)
      return MULIB_DNS_INVALID;
    if (!mulib_dnsclient_type_ (type))
      return MULIB_DNS_UNSUPPORTED;
    memset (&q, 0, sizeof (q));
    q.type = type;
    q.class_ = MULIB_DNS_IN;
    rc = mulib_dns_name_normalize (name, n, q.name);
    if (rc != MULIB_DNS_OK)
      return rc;
    rc = mulib_dns_writer_init (&w, packet, cap, id,
                                recursion ? MULIB_DNS_RD : 0);
    if (rc == MULIB_DNS_OK)
      rc = mulib_dns_writer_question (&w, &q);
    if (rc == MULIB_DNS_OK)
      *written = w.length;
    return rc;
  }
  int
  mulib_dnsclient_validate_query (const mulib_dns_message_t *m,
                                  mulib_dns_question_t *out)
  {
    mulib_dns_iterator_t it;
    mulib_dns_question_t q;
    int rc;
    if (!m || !out)
      return MULIB_DNS_INVALID;
    if ((m->header.flags & (uint16_t)~MULIB_DNS_RD) || m->header.counts[0] != 1
        || m->header.counts[1] || m->header.counts[2] || m->header.counts[3])
      return MULIB_DNS_UNSUPPORTED;
    mulib_dns_iterator_init (&it, m);
    rc = mulib_dns_next_question (&it, &q);
    if (rc != MULIB_DNS_OK)
      return rc;
    if (q.class_ != MULIB_DNS_IN || !mulib_dnsclient_type_ (q.type))
      return MULIB_DNS_UNSUPPORTED;
    *out = q;
    return MULIB_DNS_OK;
  }
  static int
  mulib_dnsclient_reply_ (const mulib_dns_header_t *header,
                          const mulib_dns_question_t *expected,
                          const mulib_dns_message_t *r)
  {
    mulib_dns_question_t actual;
    mulib_dns_iterator_t it;
    unsigned flags;
    int rc;
    if (!r)
      return MULIB_DNS_INVALID;
    flags = r->header.flags;
    if (!(flags & MULIB_DNS_QR) || (flags & ~0x878fu))
      return MULIB_DNS_UNSUPPORTED;
    if (header->id != r->header.id || r->header.counts[0] != 1
        || (header->flags & MULIB_DNS_RD) != (flags & MULIB_DNS_RD))
      return MULIB_DNS_MISMATCH;
    mulib_dns_iterator_init (&it, r);
    rc = mulib_dns_next_question (&it, &actual);
    if (rc != MULIB_DNS_OK)
      return rc;
    if (actual.type != expected->type || actual.class_ != expected->class_
        || !mulib_dns_name_equal (actual.name, expected->name))
      return MULIB_DNS_MISMATCH;
    if (flags & MULIB_DNS_TC)
      return MULIB_DNS_TRUNCATED;
    if ((flags & 15u) == 3u)
      return MULIB_DNS_NOTFOUND;
    return flags & 15u ? MULIB_DNS_RESPONSE : MULIB_DNS_OK;
  }
  int
  mulib_dnsclient_validate_reply (const mulib_dns_message_t *q,
                                  const mulib_dns_message_t *r)
  {
    mulib_dns_question_t expected;
    int rc;
    rc = mulib_dnsclient_validate_query (q, &expected);
    if (rc != MULIB_DNS_OK)
      return rc;
    return mulib_dnsclient_reply_ (&q->header, &expected, r);
  }
  int
  mulib_dnsclient_exchange (mulib_dnsclient_transport_fn fn, void *user,
                            const unsigned char *query, size_t qn,
                            unsigned char *reply, size_t cap,
                            size_t *reply_length, mulib_dns_message_t *out)
  {
    mulib_dns_message_t q, r;
    mulib_dns_question_t question;
    size_t n = 0;
    int rc;
    if (!fn || !reply || !reply_length || !out)
      return MULIB_DNS_INVALID;
    rc = mulib_dns_message_parse (query, qn, 0, &q);
    if (rc != MULIB_DNS_OK)
      return rc;
    rc = mulib_dnsclient_validate_query (&q, &question);
    if (rc != MULIB_DNS_OK)
      return rc;
    if (cap > 65535)
      cap = 65535;
    rc = fn (user, query, qn, reply, cap, &n);
    if (rc != MULIB_DNS_OK)
      return rc < 0 ? rc : MULIB_DNS_TRANSPORT;
    if (n > cap)
      return MULIB_DNS_LIMIT;
    rc = mulib_dns_message_parse (reply, n, 0, &r);
    if (rc != MULIB_DNS_OK)
      return rc;
    rc = mulib_dnsclient_reply_ (&q.header, &question, &r);
    if (rc != MULIB_DNS_OK)
      return rc;
    *reply_length = n;
    *out = r;
    return MULIB_DNS_OK;
  }
#ifdef __cplusplus
}
#endif
#endif
