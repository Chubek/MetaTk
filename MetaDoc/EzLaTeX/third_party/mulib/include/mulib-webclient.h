/**
 * @file mulib-webclient.h
 * @brief Bounded HTTP/1.1 transactions over an application-supplied transport.
 * Define MULIB_IMPL_WEBCLIENT once; dependencies are never enabled implicitly.
 */
#ifndef MULIB_WEBCLIENT_H
#define MULIB_WEBCLIENT_H
#include "mulib-http.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Transaction result; HTTP syntax/allocation result values are
   * shared. */
  typedef enum mulib_webclient_result
  {
    MULIB_WEBCLIENT_OK = 1,
    MULIB_WEBCLIENT_INVALID = -1,
    MULIB_WEBCLIENT_LIMIT = -2,
    MULIB_WEBCLIENT_NOMEM = -3,
    MULIB_WEBCLIENT_TRANSPORT = -4
  } mulib_webclient_result;
  /** @brief Borrowed receive sink. Return 1 to accept, 0 to abort. */
  typedef int (*mulib_webclient_receive_fn) (void *user, const void *bytes,
                                             size_t length);
  /** @brief Synchronous transport: send all request bytes, deliver response
   * chunks, then return 1 only after response EOF. All pointers are borrowed
   * for this call. Stop on receive failure. The endpoint is parsed from an
   * http URL (no TLS).
   */
  typedef int (*mulib_webclient_transport_fn) (
      void *user, const mulib_uri_t *endpoint, const void *request,
      size_t length, mulib_webclient_receive_fn receive, void *receiver);
  /** @brief Zero fields select HTTP defaults and a 32 MiB raw response limit.
   * http.flags must be zero; response HEAD/EOF context is supplied internally.
   */
  typedef struct mulib_webclient_options
  {
    mulib_http_options_t http;
    size_t max_response_bytes;
  } mulib_webclient_options_t;
  /** @brief Generate origin-form HTTP/1.1 with Host and Connection: close, run
   * transport, and parse one final response (up to eight informational
   * responses). Only absolute http URLs without userinfo/fragments and nonzero
   * valid ports are supported. CONNECT, Host and Connection overrides are
   * rejected. Inputs are borrowed; on success response owns allocations, on
   * error unchanged. response must be empty/uninitialized, not an existing
   * owned message.
   */
  mulib_webclient_result mulib_webclient_request (
      const char *url, const char *method, const mulib_http_header_t *headers,
      size_t header_count, const void *body, size_t body_length,
      const mulib_webclient_options_t *options,
      mulib_webclient_transport_fn transport, void *user,
      mulib_http_message_t *response);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_WEBCLIENT) && !defined(MULIB_WEBCLIENT_IMPLEMENTED)
#define MULIB_WEBCLIENT_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_webclient_equal_ (const char *a, const char *b)
  {
    if (!a || !b)
      return 0;
    while (*a && *b)
      {
        unsigned char x = (unsigned char)*a++, y = (unsigned char)*b++;
        if (x >= 'A' && x <= 'Z')
          x = (unsigned char)(x + 32);
        if (y >= 'A' && y <= 'Z')
          y = (unsigned char)(y + 32);
        if (x != y)
          return 0;
      }
    return *a == *b;
  }
  typedef struct mulib_webclient_sink_
  {
    mulib_buffer_t bytes;
    size_t limit;
    mulib_webclient_result error;
  } mulib_webclient_sink_;
  static int
  mulib_webclient_receive_ (void *user, const void *bytes, size_t length)
  {
    mulib_webclient_sink_ *s = (mulib_webclient_sink_ *)user;
    if (s->error != MULIB_WEBCLIENT_OK)
      return 0;
    if (!bytes && length)
      s->error = MULIB_WEBCLIENT_INVALID;
    else if (length > s->limit - s->bytes.length)
      s->error = MULIB_WEBCLIENT_LIMIT;
    else if (!mulib_buffer_append (&s->bytes, bytes, length))
      s->error = MULIB_WEBCLIENT_NOMEM;
    return s->error == MULIB_WEBCLIENT_OK;
  }
  mulib_webclient_result
  mulib_webclient_request (const char *url, const char *method,
                           const mulib_http_header_t *headers, size_t count,
                           const void *body, size_t body_length,
                           const mulib_webclient_options_t *options,
                           mulib_webclient_transport_fn transport, void *user,
                           mulib_http_message_t *response)
  {
    mulib_uri_t uri;
    mulib_strbuf_t host, target;
    mulib_http_message_t request, parsed;
    mulib_http_options_t limits = { 0, 0, 0, 0 };
    mulib_webclient_sink_ sink;
    mulib_http_header_t *fields = NULL;
    unsigned char *wire = NULL;
    size_t wire_length = 0, offset = 0, consumed = 0;
    mulib_webclient_result result = MULIB_WEBCLIENT_INVALID;
    unsigned port = 0, informational = 0;
    if (!url || !method || !transport || !response || (!headers && count)
        || (!body && body_length) || !strcmp (method, "CONNECT"))
      return result;
    if (options)
      limits = options->http;
    if (limits.flags || !mulib_uri_parse (url, strlen (url), &uri)
        || uri.scheme.length != 4 || !uri.has_authority || !uri.host.length
        || uri.has_userinfo || uri.has_fragment)
      return result;
    for (size_t i = 0; i < 4; ++i)
      {
        unsigned char c = (unsigned char)uri.scheme.data[i];
        if (c >= 'A' && c <= 'Z')
          c = (unsigned char)(c + 32);
        if (c != (unsigned char)"http"[i])
          return result;
      }
    if (uri.has_port)
      {
        if (!uri.port.length)
          return result;
        for (size_t i = 0; i < uri.port.length; ++i)
          {
            unsigned d = (unsigned)(uri.port.data[i] - '0');
            if (port > (65535u - d) / 10u)
              return result;
            port = port * 10u + d;
          }
        if (!port)
          return result;
      }
    if (count > SIZE_MAX / sizeof (*fields) - 2)
      return MULIB_WEBCLIENT_LIMIT;
    if (count > (limits.max_headers ? limits.max_headers : 100)
        || (limits.max_headers ? limits.max_headers : 100) - count < 2)
      return MULIB_WEBCLIENT_LIMIT;
    for (size_t i = 0; i < count; ++i)
      {
        if (!headers[i].name || !headers[i].value
            || mulib_webclient_equal_ (headers[i].name, "Host")
            || mulib_webclient_equal_ (headers[i].name, "Connection"))
          return result;
      }
    mulib_strbuf_init (&host);
    mulib_strbuf_init (&target);
    memset (&request, 0, sizeof (request));
    memset (&parsed, 0, sizeof (parsed));
    mulib_buffer_init (&sink.bytes);
    sink.error = MULIB_WEBCLIENT_OK;
    sink.limit = options && options->max_response_bytes
                     ? options->max_response_bytes
                     : 32u * 1024u * 1024u;
    result = MULIB_WEBCLIENT_NOMEM;
    if (!mulib_strbuf_append_n (&host, uri.host.data, uri.host.length)
        || (uri.has_port
            && (!mulib_strbuf_putc (&host, ':')
                || !mulib_strbuf_append_n (&host, uri.port.data,
                                           uri.port.length)))
        || !mulib_strbuf_append_n (&target,
                                   uri.path.length ? uri.path.data : "/",
                                   uri.path.length ? uri.path.length : 1)
        || (uri.has_query
            && (!mulib_strbuf_putc (&target, '?')
                || !mulib_strbuf_append_n (&target, uri.query.data,
                                           uri.query.length))))
      goto done;
    fields = (mulib_http_header_t *)malloc ((count + 2) * sizeof (*fields));
    if (!fields)
      goto done;
    if (count)
      memcpy (fields, headers, count * sizeof (*fields));
    fields[count].name = (char *)"Host";
    fields[count].value = host.data;
    fields[count + 1].name = (char *)"Connection";
    fields[count + 1].value = (char *)"close";
    request.method = (char *)method;
    request.target = target.data;
    request.headers = fields;
    request.header_count = count + 2;
    request.body = (unsigned char *)body;
    request.body_length = body_length;
    result = (mulib_webclient_result)mulib_http_serialize (
        &request, &limits, &wire, &wire_length);
    if (result != MULIB_WEBCLIENT_OK)
      goto done;
    {
      int ok = transport (user, &uri, wire, wire_length,
                          mulib_webclient_receive_, &sink);
      if (sink.error != MULIB_WEBCLIENT_OK)
        {
          result = sink.error;
          goto done;
        }
      if (ok != 1)
        {
          result = MULIB_WEBCLIENT_TRANSPORT;
          goto done;
        }
    }
    limits.flags = MULIB_HTTP_EOF;
    if (!strcmp (method, "HEAD"))
      limits.flags |= MULIB_HTTP_HEAD_RESPONSE;
    for (;;)
      {
        const void *data = sink.bytes.length ? sink.bytes.data + offset : NULL;
        result = (mulib_webclient_result)mulib_http_parse (
            data, sink.bytes.length - offset, 1, &limits, &parsed, &consumed);
        if (result != MULIB_WEBCLIENT_OK)
          goto done;
        offset += consumed;
        if (parsed.status >= 200)
          break;
        if (parsed.status == 101 || ++informational > 8)
          {
            result = MULIB_WEBCLIENT_INVALID;
            goto done;
          }
        mulib_http_message_free (&parsed);
      }
    if (offset != sink.bytes.length)
      {
        result = MULIB_WEBCLIENT_INVALID;
        goto done;
      }
    *response = parsed;
    memset (&parsed, 0, sizeof (parsed));
  done:
    mulib_http_message_free (&parsed);
    free (wire);
    free (fields);
    mulib_strbuf_free (&host);
    mulib_strbuf_free (&target);
    mulib_buffer_free (&sink.bytes);
    return result;
  }
#ifdef __cplusplus
}
#endif
#endif
