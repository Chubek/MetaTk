/** @file mulib-revproxy.h
 * @brief Fixed-origin, transport-independent HTTP reverse proxy transaction.
 */
#ifndef MULIB_REVPROXY_H
#define MULIB_REVPROXY_H
#include "mulib-webclient.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Forward a validated origin-form request to an http origin (no
   * prefix path/query/userinfo/fragment). The decoded binary body is reframed.
   * Host is replaced; standard hop-by-hop and every Connection-nominated field
   * are removed in both directions. CONNECT and Upgrade requests are rejected,
   * not tunneled. All inputs are borrowed. On OK response owns allocations;
   * otherwise unchanged. The origin and transport MUST be trusted/configured
   * by the application.
   */
  mulib_webclient_result
  mulib_revproxy_forward (const char *origin,
                          const mulib_http_message_t *request,
                          const mulib_webclient_options_t *options,
                          mulib_webclient_transport_fn transport, void *user,
                          mulib_http_message_t *response);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_REVPROXY) && !defined(MULIB_REVPROXY_IMPLEMENTED)
#define MULIB_REVPROXY_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_revproxy_equal_ (const char *a, size_t n, const char *b)
  {
    if (strlen (b) != n)
      return 0;
    for (size_t i = 0; i < n; ++i)
      {
        unsigned char x = (unsigned char)a[i], y = (unsigned char)b[i];
        if (x >= 'A' && x <= 'Z')
          x = (unsigned char)(x + 32);
        if (y >= 'A' && y <= 'Z')
          y = (unsigned char)(y + 32);
        if (x != y)
          return 0;
      }
    return 1;
  }
  static int
  mulib_revproxy_token_ (unsigned char c)
  {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
           || (c >= '0' && c <= '9') || (c && strchr ("!#$%&'*+-.^_`|~", c));
  }
  /* Validate the whole Connection list even after finding a matching token. */
  static int
  mulib_revproxy_connection_ (const char *value, const char *name)
  {
    const char *p = value;
    int found = 0;
    for (;;)
      {
        const char *start;
        size_t n;
        while (*p == ' ' || *p == '\t')
          ++p;
        start = p;
        while (mulib_revproxy_token_ ((unsigned char)*p))
          ++p;
        n = (size_t)(p - start);
        if (!n)
          return -1;
        if (name && mulib_revproxy_equal_ (start, n, name))
          found = 1;
        while (*p == ' ' || *p == '\t')
          ++p;
        if (!*p)
          return found;
        if (*p++ != ',')
          return -1;
      }
  }
  static mulib_webclient_result
  mulib_revproxy_filter_ (const mulib_http_message_t *m, int request,
                          int metadata, mulib_http_header_t **out,
                          size_t *count)
  {
    static const char *const hop[] = { "Connection",
                                       "Keep-Alive",
                                       "Proxy-Connection",
                                       "Proxy-Authenticate",
                                       "Proxy-Authorization",
                                       "TE",
                                       "Trailer",
                                       "Transfer-Encoding",
                                       "Upgrade" };
    mulib_http_header_t *fields;
    size_t used = 0;
    if (m->header_count > SIZE_MAX / sizeof (*fields))
      return MULIB_WEBCLIENT_LIMIT;
    for (size_t i = 0; i < m->header_count; ++i)
      if (mulib_revproxy_equal_ (m->headers[i].name,
                                 strlen (m->headers[i].name), "Connection")
          && mulib_revproxy_connection_ (m->headers[i].value, NULL) < 0)
        return MULIB_WEBCLIENT_INVALID;
    fields = m->header_count ? (mulib_http_header_t *)malloc (
                                   m->header_count * sizeof (*fields))
                             : NULL;
    if (!fields && m->header_count)
      return MULIB_WEBCLIENT_NOMEM;
    for (size_t i = 0; i < m->header_count; ++i)
      {
        const char *name = m->headers[i].name;
        size_t n = strlen (name);
        int drop = 0;
        for (size_t j = 0; j < sizeof (hop) / sizeof (hop[0]); ++j)
          if (mulib_revproxy_equal_ (name, n, hop[j]))
            drop = 1;
        if (request && mulib_revproxy_equal_ (name, n, "Host"))
          drop = 1;
        if (!metadata && mulib_revproxy_equal_ (name, n, "Content-Length"))
          drop = 1;
        for (size_t j = 0; j < m->header_count; ++j)
          if (mulib_revproxy_equal_ (m->headers[j].name,
                                     strlen (m->headers[j].name), "Connection")
              && mulib_revproxy_connection_ (m->headers[j].value, name) == 1)
            drop = 1;
        if (!drop)
          fields[used++] = m->headers[i];
      }
    *out = fields;
    *count = used;
    return MULIB_WEBCLIENT_OK;
  }
  mulib_webclient_result
  mulib_revproxy_forward (const char *origin,
                          const mulib_http_message_t *request,
                          const mulib_webclient_options_t *options,
                          mulib_webclient_transport_fn transport, void *user,
                          mulib_http_message_t *out)
  {
    mulib_uri_t base, target;
    mulib_strbuf_t url;
    mulib_http_options_t limits = { 0, 0, 0, 0 };
    mulib_http_message_t upstream, filtered;
    mulib_http_header_t *fields = NULL;
    unsigned char *wire = NULL;
    size_t n = 0, count = 0, used = 0, origin_length;
    mulib_webclient_result rc;
    if (!origin || !request || !out || !transport || request->is_response)
      return MULIB_WEBCLIENT_INVALID;
    if (options)
      limits = options->http;
    if (limits.flags)
      return MULIB_WEBCLIENT_INVALID;
    rc = (mulib_webclient_result)mulib_http_serialize (request, &limits, &wire,
                                                       &n);
    free (wire);
    wire = NULL;
    if (rc != MULIB_WEBCLIENT_OK)
      return rc;
    if (!strcmp (request->method, "CONNECT")
        || mulib_http_header (request, "Upgrade") || request->target[0] != '/'
        || !mulib_uri_parse (request->target, strlen (request->target),
                             &target)
        || target.scheme.length || target.has_authority || target.has_fragment
        || !mulib_uri_parse (origin, strlen (origin), &base)
        || !base.has_authority || base.has_userinfo || base.has_query
        || base.has_fragment
        || (base.path.length
            && (base.path.length != 1 || base.path.data[0] != '/')))
      return MULIB_WEBCLIENT_INVALID;
    rc = mulib_revproxy_filter_ (request, 1, 0, &fields, &count);
    if (rc != MULIB_WEBCLIENT_OK)
      return rc;
    mulib_strbuf_init (&url);
    memset (&upstream, 0, sizeof (upstream));
    origin_length = strlen (origin) - base.path.length;
    if (!mulib_strbuf_append_n (&url, origin, origin_length)
        || !mulib_strbuf_append (&url, request->target))
      {
        rc = MULIB_WEBCLIENT_NOMEM;
        goto done;
      }
    rc = mulib_webclient_request (url.data, request->method, fields, count,
                                  request->body, request->body_length, options,
                                  transport, user, &upstream);
    free (fields);
    fields = NULL;
    if (rc != MULIB_WEBCLIENT_OK)
      goto done;
    rc = mulib_revproxy_filter_ (&upstream, 0,
                                 !strcmp (request->method, "HEAD")
                                     || upstream.status == 304,
                                 &fields, &count);
    if (rc != MULIB_WEBCLIENT_OK)
      goto done;
    filtered = upstream;
    filtered.headers = fields;
    filtered.header_count = count;
    filtered.chunked = 0;
    filtered.close_delimited = 0;
    limits.flags
        = !strcmp (request->method, "HEAD") ? MULIB_HTTP_HEAD_RESPONSE : 0;
    rc = (mulib_webclient_result)mulib_http_serialize (&filtered, &limits,
                                                       &wire, &n);
    limits.flags |= MULIB_HTTP_EOF;
    if (rc == MULIB_WEBCLIENT_OK)
      rc = (mulib_webclient_result)mulib_http_parse (wire, n, 1, &limits, out,
                                                     &used);
  done:
    free (wire);
    free (fields);
    mulib_http_message_free (&upstream);
    mulib_strbuf_free (&url);
    return rc;
  }
#ifdef __cplusplus
}
#endif
#endif
