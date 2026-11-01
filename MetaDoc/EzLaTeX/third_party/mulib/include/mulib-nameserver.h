/**
 * @file mulib-nameserver.h
 * @brief Static single-zone authoritative DNS answers, without network I/O.
 * Define MULIB_IMPL_NAMESERVER, MULIB_IMPL_DNSCLIENT and MULIB_IMPL_DNS
 * separately.
 */
#ifndef MULIB_NAMESERVER_H
#define MULIB_NAMESERVER_H
#include "mulib-dnsclient.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Borrows immutable records/RDATA; owns its normalized zone origin.
   */
  typedef struct mulib_nameserver
  {
    char origin[256];
    const mulib_dns_rr_t *records;
    size_t count;
  } mulib_nameserver_t;
  /** @brief Validate one zone, IN records, supported RDATA, and CNAME
   * exclusivity. */
  int mulib_nameserver_init (mulib_nameserver_t *server, const char *origin,
                             size_t length, const mulib_dns_rr_t *records,
                             size_t count);
  /**
   * @brief Answer a validated standard query: AA, NXDOMAIN, NODATA or REFUSED.
   * No allocation, wildcards, delegation, recursion, glue or SOA/negative
   * TTLs. Complete RRs that fit are emitted; the first omitted RR sets TC.
   * Query and output storage must not overlap. written changes only on
   * success.
   */
  int mulib_nameserver_answer (const mulib_nameserver_t *server,
                               const unsigned char *query, size_t query_length,
                               unsigned char *reply, size_t reply_capacity,
                               size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NAMESERVER) && !defined(MULIB_NAMESERVER_IMPLEMENTED)
#define MULIB_NAMESERVER_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_nameserver_in_zone_ (const char *name, const char *origin)
  {
    size_t n = strlen (name), z = strlen (origin);
    if (z == 1 && origin[0] == '.')
      return 1;
    return n >= z && !strcmp (name + n - z, origin)
           && (n == z || name[n - z - 1] == '.');
  }
  int
  mulib_nameserver_init (mulib_nameserver_t *s, const char *origin, size_t n,
                         const mulib_dns_rr_t *records, size_t count)
  {
    mulib_nameserver_t value;
    int rc;
    if (!s || (count && !records))
      return MULIB_DNS_INVALID;
    if (count >= MULIB_DNS_MAX_RECORDS)
      return MULIB_DNS_LIMIT;
    memset (&value, 0, sizeof (value));
    rc = mulib_dns_name_normalize (origin, n, value.origin);
    if (rc != MULIB_DNS_OK)
      return rc;
    for (size_t i = 0; i < count; ++i)
      {
        char name[256];
        size_t length = 0;
        rc = mulib_dns_rr_validate (&records[i]);
        if (rc != MULIB_DNS_OK)
          return rc;
        if (records[i].class_ != MULIB_DNS_IN)
          return MULIB_DNS_UNSUPPORTED;
        while (length < 256 && records[i].name[length])
          ++length;
        rc = mulib_dns_name_normalize (records[i].name, length, name);
        if (rc != MULIB_DNS_OK)
          return rc;
        if (!mulib_nameserver_in_zone_ (name, value.origin))
          return MULIB_DNS_INVALID;
        for (size_t j = 0; j < i; ++j)
          {
            if ((records[i].type == MULIB_DNS_CNAME
                 || records[j].type == MULIB_DNS_CNAME)
                && mulib_dns_name_equal (records[i].name, records[j].name))
              return MULIB_DNS_INVALID;
          }
      }
    value.records = records;
    value.count = count;
    *s = value;
    return MULIB_DNS_OK;
  }
  int
  mulib_nameserver_answer (const mulib_nameserver_t *s,
                           const unsigned char *query, size_t qn,
                           unsigned char *reply, size_t cap, size_t *written)
  {
    mulib_dns_message_t q;
    mulib_dns_question_t question;
    mulib_dns_writer_t w;
    uint16_t flags;
    int rc, inside, exists = 0;
    if (!s || !reply || !written || (s->count && !s->records)
        || s->count >= MULIB_DNS_MAX_RECORDS)
      return MULIB_DNS_INVALID;
    rc = mulib_dns_message_parse (query, qn, 0, &q);
    if (rc != MULIB_DNS_OK)
      return rc;
    rc = mulib_dnsclient_validate_query (&q, &question);
    if (rc != MULIB_DNS_OK)
      return rc;
    inside = mulib_nameserver_in_zone_ (question.name, s->origin);
    if (inside)
      {
        exists = mulib_dns_name_equal (s->origin, question.name);
        for (size_t i = 0; i < s->count && !exists; ++i)
          {
            char owner[256];
            size_t length = 0;
            while (length < 256 && s->records[i].name[length])
              ++length;
            rc = mulib_dns_name_normalize (s->records[i].name, length, owner);
            if (rc != MULIB_DNS_OK)
              return rc;
            if (mulib_nameserver_in_zone_ (owner, question.name))
              exists = 1;
          }
      }
    flags = (uint16_t)(MULIB_DNS_QR | (q.header.flags & MULIB_DNS_RD));
    if (inside)
      flags = (uint16_t)(flags | MULIB_DNS_AA | (exists ? 0u : 3u));
    else
      flags = (uint16_t)(flags | 5u);
    rc = mulib_dns_writer_init (&w, reply, cap, q.header.id, flags);
    if (rc == MULIB_DNS_OK)
      rc = mulib_dns_writer_question (&w, &question);
    if (rc != MULIB_DNS_OK)
      return rc;
    if (inside && exists)
      for (size_t i = 0; i < s->count; ++i)
        {
          const mulib_dns_rr_t *rr = &s->records[i];
          if (!mulib_dns_name_equal (rr->name, question.name)
              || (question.type != MULIB_DNS_ANY && rr->type != question.type
                  && rr->type != MULIB_DNS_CNAME))
            continue;
          rc = mulib_dns_writer_rr (&w, 1, rr);
          if (rc == MULIB_DNS_LIMIT)
            {
              flags = (uint16_t)(flags | MULIB_DNS_TC);
              reply[2] = (unsigned char)(flags >> 8);
              reply[3] = (unsigned char)flags;
              break;
            }
          if (rc != MULIB_DNS_OK)
            return rc;
        }
    *written = w.length;
    return MULIB_DNS_OK;
  }
#ifdef __cplusplus
}
#endif
#endif
