/** @file mulib-webframework.h
 * @brief Owned exact-path routing and response construction over mulib-http.
 */
#ifndef MULIB_WEBFRAMEWORK_H
#define MULIB_WEBFRAMEWORK_H
#include "mulib-http.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Handler borrows request/user and creates an owned response on OK.
   * The supplied response is initially zeroed. Allocations on failure are
   * freed.
   */
  typedef mulib_http_result (*mulib_webframework_handler_fn) (
      void *user, const mulib_http_message_t *request,
      mulib_http_message_t *response);
  /** @brief Owned registration strings; user and handler are borrowed. */
  typedef struct mulib_webframework_route
  {
    char *method, *path;
    mulib_webframework_handler_fn handler;
    void *user;
  } mulib_webframework_route_t;
  /** @brief Owned route array. Do not mutate fields or copy after
   * initialization. */
  typedef struct mulib_webframework
  {
    mulib_webframework_route_t *routes;
    size_t count, max_routes;
  } mulib_webframework_t;
  /** @brief Initialize an empty router; zero maximum selects 64 routes. */
  void mulib_webframework_init (mulib_webframework_t *router,
                                size_t max_routes);
  /** @brief Free registrations, not handler contexts; NULL is safe. */
  void mulib_webframework_free (mulib_webframework_t *router);
  /** @brief Copy a case-sensitive method and exact origin path (no
   * query/fragment). Duplicate registrations are INVALID. No decoding,
   * wildcards or captures.
   */
  mulib_http_result
  mulib_webframework_route (mulib_webframework_t *router, const char *method,
                            const char *path,
                            mulib_webframework_handler_fn handler, void *user);
  /** @brief Construct an owned binary response with optional Content-Type and
   * canonical Content-Length, using the HTTP serializer/parser for validation.
   * Default HTTP limits apply; output is unchanged on failure.
   */
  mulib_http_result mulib_webframework_reply (mulib_http_message_t *response,
                                              unsigned status,
                                              const char *content_type,
                                              const void *body, size_t length);
  /** @brief Validate and dispatch an origin-form request. Query is ignored for
   * matching, path escapes remain encoded. HEAD falls back to GET and
   * suppresses the response body while preserving its Content-Length.
   * Unmatched routes, including method mismatches, produce 404. Output owns
   * memory only on OK.
   */
  mulib_http_result
  mulib_webframework_dispatch (const mulib_webframework_t *router,
                               const mulib_http_message_t *request,
                               mulib_http_message_t *response);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEBFRAMEWORK)                                          \
    && !defined(MULIB_WEBFRAMEWORK_IMPLEMENTED)
#define MULIB_WEBFRAMEWORK_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static char *
  mulib_webframework_copy_ (const char *s)
  {
    size_t n = strlen (s);
    char *p;
    if (n == SIZE_MAX || !(p = (char *)malloc (n + 1)))
      return NULL;
    memcpy (p, s, n + 1);
    return p;
  }
  static int
  mulib_webframework_token_ (const char *s)
  {
    if (!s || !*s)
      return 0;
    for (; *s; ++s)
      {
        unsigned char c = (unsigned char)*s;
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
              || (c >= '0' && c <= '9') || strchr ("!#$%&'*+-.^_`|~", c)))
          return 0;
      }
    return 1;
  }
  void
  mulib_webframework_init (mulib_webframework_t *r, size_t max_routes)
  {
    if (r)
      {
        memset (r, 0, sizeof (*r));
        r->max_routes = max_routes ? max_routes : 64;
      }
  }
  void
  mulib_webframework_free (mulib_webframework_t *r)
  {
    if (!r)
      return;
    for (size_t i = 0; i < r->count; ++i)
      {
        free (r->routes[i].method);
        free (r->routes[i].path);
      }
    free (r->routes);
    memset (r, 0, sizeof (*r));
  }
  mulib_http_result
  mulib_webframework_route (mulib_webframework_t *r, const char *method,
                            const char *path,
                            mulib_webframework_handler_fn handler, void *user)
  {
    mulib_uri_t uri;
    mulib_webframework_route_t item, *next;
    if (!r || !method || !path || !handler || !r->max_routes
        || (!r->routes && r->count))
      return MULIB_HTTP_INVALID;
    if (strlen (method) > 32768 || strlen (path) > 32768)
      return MULIB_HTTP_LIMIT;
    if (!mulib_webframework_token_ (method) || path[0] != '/'
        || !mulib_uri_parse (path, strlen (path), &uri) || uri.has_authority
        || uri.scheme.length || uri.has_query || uri.has_fragment)
      return MULIB_HTTP_INVALID;
    for (size_t i = 0; i < r->count; ++i)
      if (!strcmp (r->routes[i].method, method)
          && !strcmp (r->routes[i].path, path))
        return MULIB_HTTP_INVALID;
    if (r->count >= r->max_routes || r->count >= SIZE_MAX / sizeof (*next))
      return MULIB_HTTP_LIMIT;
    item.method = mulib_webframework_copy_ (method);
    item.path = mulib_webframework_copy_ (path);
    if (!item.method || !item.path)
      {
        free (item.method);
        free (item.path);
        return MULIB_HTTP_NOMEM;
      }
    item.handler = handler;
    item.user = user;
    next = (mulib_webframework_route_t *)realloc (
        r->routes, (r->count + 1) * sizeof (*next));
    if (!next)
      {
        free (item.method);
        free (item.path);
        return MULIB_HTTP_NOMEM;
      }
    r->routes = next;
    r->routes[r->count++] = item;
    return MULIB_HTTP_OK;
  }
  mulib_http_result
  mulib_webframework_reply (mulib_http_message_t *out, unsigned status,
                            const char *type, const void *body, size_t length)
  {
    mulib_http_message_t m;
    mulib_http_header_t h;
    unsigned char *wire = NULL;
    size_t n = 0, used = 0;
    mulib_http_result rc;
    mulib_http_options_t options = { 0, 0, 0, MULIB_HTTP_EOF };
    if (!out)
      return MULIB_HTTP_INVALID;
    memset (&m, 0, sizeof (m));
    m.is_response = 1;
    m.status = status;
    switch (status)
      {
      case 200:
        m.reason = (char *)"OK";
        break;
      case 201:
        m.reason = (char *)"Created";
        break;
      case 204:
        m.reason = (char *)"No Content";
        break;
      case 400:
        m.reason = (char *)"Bad Request";
        break;
      case 403:
        m.reason = (char *)"Forbidden";
        break;
      case 404:
        m.reason = (char *)"Not Found";
        break;
      case 405:
        m.reason = (char *)"Method Not Allowed";
        break;
      case 413:
        m.reason = (char *)"Content Too Large";
        break;
      case 500:
        m.reason = (char *)"Internal Server Error";
        break;
      default:
        m.reason = (char *)"Response";
        break;
      }
    h.name = (char *)"Content-Type";
    h.value = (char *)type;
    if (type)
      {
        m.headers = &h;
        m.header_count = 1;
      }
    m.body = (unsigned char *)body;
    m.body_length = length;
    rc = mulib_http_serialize (&m, &options, &wire, &n);
    if (rc == MULIB_HTTP_OK)
      rc = mulib_http_parse (wire, n, 1, &options, out, &used);
    free (wire);
    return rc;
  }
  mulib_http_result
  mulib_webframework_dispatch (const mulib_webframework_t *r,
                               const mulib_http_message_t *request,
                               mulib_http_message_t *out)
  {
    mulib_uri_t uri;
    const mulib_webframework_route_t *match = NULL;
    mulib_http_message_t response;
    unsigned char *wire = NULL;
    size_t n = 0;
    mulib_http_options_t options = { 0, 0, 0, 0 };
    mulib_http_result rc;
    if (!r || !request || !out || request->is_response
        || (!r->routes && r->count))
      return MULIB_HTTP_INVALID;
    rc = mulib_http_serialize (request, NULL, &wire, &n);
    free (wire);
    wire = NULL;
    if (rc != MULIB_HTTP_OK)
      return rc;
    if (request->target[0] != '/'
        || !mulib_uri_parse (request->target, strlen (request->target), &uri)
        || uri.has_authority || uri.scheme.length || uri.has_fragment)
      return MULIB_HTTP_INVALID;
    for (size_t i = 0; i < r->count; ++i)
      {
        const mulib_webframework_route_t *route = &r->routes[i];
        if (strlen (route->path) != uri.path.length
            || memcmp (route->path, uri.path.data, uri.path.length))
          continue;
        if (!strcmp (route->method, request->method))
          {
            match = route;
            break;
          }
        if (!strcmp (request->method, "HEAD")
            && !strcmp (route->method, "GET"))
          match = route;
      }
    memset (&response, 0, sizeof (response));
    rc = match ? match->handler (match->user, request, &response)
               : mulib_webframework_reply (&response, 404, "text/plain",
                                           "Not Found\n", 10);
    if (rc != MULIB_HTTP_OK)
      {
        mulib_http_message_free (&response);
        return rc == MULIB_HTTP_MORE ? MULIB_HTTP_INVALID : rc;
      }
    if (!strcmp (request->method, "HEAD"))
      {
        /* Canonicalize first to retain a generated representation length. */
        if (response.body_length)
          {
            mulib_http_message_t canonical;
            size_t used = 0;
            memset (&canonical, 0, sizeof (canonical));
            rc = mulib_http_serialize (&response, NULL, &wire, &n);
            options.flags = MULIB_HTTP_EOF;
            if (rc == MULIB_HTTP_OK)
              rc = mulib_http_parse (wire, n, 1, &options, &canonical, &used);
            free (wire);
            wire = NULL;
            if (rc != MULIB_HTTP_OK)
              {
                mulib_http_message_free (&response);
                return rc;
              }
            mulib_http_message_free (&response);
            response = canonical;
          }
        free (response.body);
        response.body = NULL;
        response.body_length = 0;
        /* A chunked GET cannot be represented as a HEAD under this HTTP
         * policy. */
        options.flags = MULIB_HTTP_HEAD_RESPONSE;
      }
    else
      options.flags = 0;
    if (!response.is_response)
      rc = MULIB_HTTP_INVALID;
    else
      rc = mulib_http_serialize (&response, &options, &wire, &n);
    free (wire);
    if (rc == MULIB_HTTP_OK)
      *out = response;
    else
      mulib_http_message_free (&response);
    return rc;
  }
#ifdef __cplusplus
}
#endif
#endif
