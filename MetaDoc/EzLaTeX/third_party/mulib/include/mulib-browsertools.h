/**
 * @file mulib-browsertools.h
 * @brief Callback-driven CDP request building, session matching and target
 * inspection. No browser, sockets, WebSocket transport or JavaScript execution
 * is built in. Define MULIB_IMPL_BROWSERTOOLS once, plus
 * JSON/PARSEC/STRBUF/UTF8 implementations.
 */
#ifndef MULIB_BROWSERTOOLS_H
#define MULIB_BROWSERTOOLS_H
#include "mulib-http.h"
#include "mulib-json.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_browsertools mulib_browsertools_t;
  typedef enum mulib_browsertools_status
  {
    MULIB_BROWSERTOOLS_OK = 0,
    MULIB_BROWSERTOOLS_RESPONSE = 1,
    MULIB_BROWSERTOOLS_EVENT = 2,
    MULIB_BROWSERTOOLS_UNMATCHED = 3,
    MULIB_BROWSERTOOLS_INVALID = -1,
    MULIB_BROWSERTOOLS_LIMIT = -2,
    MULIB_BROWSERTOOLS_MEMORY = -3,
    MULIB_BROWSERTOOLS_TRANSPORT = -4
  } mulib_browsertools_status_t;
  /** @brief Callback borrows one UTF-8 JSON message until return; 0 means
   * accepted. No reentry into the client is allowed. Application owns
   * transport and framing.
   */
  typedef int (*mulib_browsertools_send_fn) (void *user, const char *json,
                                             size_t length);
  /** @brief Hard limits; NULL selects 128 pending requests and 1 MiB per
   * message. */
  typedef struct mulib_browsertools_limits
  {
    size_t pending, message_bytes;
  } mulib_browsertools_limits_t;
  /** @brief Owned envelope, initialized to zero; result/error/params borrow
   * from json. */
  typedef struct mulib_browsertools_message
  {
    mulib_json_t *json;
    uint32_t id;
    int remote_error;
  } mulib_browsertools_message_t;
  /** @brief Allocate a client. One client corresponds to one CDP transport
   * connection. */
  mulib_browsertools_t *
  mulib_browsertools_create (mulib_browsertools_send_fn send, void *user,
                             const mulib_browsertools_limits_t *limits);
  /** @brief Release client and pending session names. Does not close
   * transport. */
  void mulib_browsertools_destroy (mulib_browsertools_t *client);
  /** @brief Copy/serialize borrowed object params, send and register a
   * request. session is NULL for root or a nonempty flattened CDP session ID.
   * id is written on success only. Failed sends consume an ID but do not
   * occupy a pending slot.
   */
  mulib_browsertools_status_t
  mulib_browsertools_request (mulib_browsertools_t *client, const char *method,
                              const mulib_json_t *params, const char *session,
                              uint32_t *id);
  /** @brief Parse a complete envelope, retire an exactly matching id+session
   * response. RESPONSE includes remote error envelopes; inspect remote_error
   * and json.error. Valid unmatched responses and events are also returned as
   * owned messages. Malformed/oversized input leaves out unchanged and never
   * retires a request.
   */
  mulib_browsertools_status_t
  mulib_browsertools_receive (mulib_browsertools_t *client, const char *json,
                              size_t length,
                              mulib_browsertools_message_t *out);
  /** @brief Release and clear an envelope. NULL is harmless. */
  void mulib_browsertools_message_free (mulib_browsertools_message_t *message);
  /** @brief Cancel local tracking by ID (e.g. timeout); no remote
   * cancellation. */
  int mulib_browsertools_cancel (mulib_browsertools_t *client, uint32_t id);
  /** @brief Current pending request count. */
  size_t mulib_browsertools_pending (const mulib_browsertools_t *client);
  /** @brief Send Page.navigate with JSON-escaped URL; browser validates URL
   * policy. */
  mulib_browsertools_status_t
  mulib_browsertools_navigate (mulib_browsertools_t *client, const char *url,
                               const char *session, uint32_t *id);
  /** @brief Send Runtime.evaluate with returnByValue and awaitPromise true.
   * Expression is arbitrary remote code: never pass untrusted input without
   * policy.
   */
  mulib_browsertools_status_t
  mulib_browsertools_evaluate (mulib_browsertools_t *client,
                               const char *expression, const char *session,
                               uint32_t *id);
  /** @brief Parse /json/list's borrowed HTTP 200 body into an owned target
   * array. Each target must have string id/type/url. No HTTP functions or
   * transport used.
   */
  mulib_json_t *
  mulib_browsertools_targets (const mulib_http_message_t *response,
                              const mulib_browsertools_limits_t *limits);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_BROWSERTOOLS)                                          \
    && !defined(MULIB_BROWSERTOOLS_IMPLEMENTED)
#define MULIB_BROWSERTOOLS_IMPLEMENTED
#include "mulib-utf8.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_browsertools_pending_
  {
    uint32_t id;
    char *session;
  } mulib_browsertools_pending_;
  struct mulib_browsertools
  {
    mulib_browsertools_send_fn send;
    void *user;
    mulib_browsertools_limits_t limits;
    mulib_browsertools_pending_ *pending;
    size_t count;
    uint32_t next_id;
  };
  static mulib_browsertools_limits_t
  mulib_browsertools_limits_ (const mulib_browsertools_limits_t *l)
  {
    mulib_browsertools_limits_t d = { 128, 1048576 };
    return l ? *l : d;
  }
  mulib_browsertools_t *
  mulib_browsertools_create (mulib_browsertools_send_fn send, void *user,
                             const mulib_browsertools_limits_t *limits)
  {
    mulib_browsertools_limits_t l = mulib_browsertools_limits_ (limits);
    mulib_browsertools_t *c;
    if (!send || l.pending > SIZE_MAX / sizeof (mulib_browsertools_pending_))
      return NULL;
    c = (mulib_browsertools_t *)calloc (1, sizeof *c);
    if (!c)
      return NULL;
    c->send = send;
    c->user = user;
    c->limits = l;
    c->next_id = 1;
    if (l.pending)
      c->pending = (mulib_browsertools_pending_ *)malloc (
          l.pending * sizeof *c->pending);
    if (l.pending && !c->pending)
      {
        free (c);
        return NULL;
      }
    return c;
  }
  void
  mulib_browsertools_destroy (mulib_browsertools_t *c)
  {
    size_t i;
    if (!c)
      return;
    for (i = 0; i < c->count; ++i)
      free (c->pending[i].session);
    free (c->pending);
    free (c);
  }
  static int
  mulib_browsertools_set_ (mulib_json_t *o, const char *key, mulib_json_t *v)
  {
    if (v && mulib_json_set (o, key, v))
      return 1;
    mulib_json_destroy (v);
    return 0;
  }
  /* Bound traversal and temporary JSON copying before serializing caller-built
   * trees. Wire may expand strings sixfold, then the exact message cap is
   * checked. */
  static int
  mulib_browsertools_tree_ (const mulib_json_t *v, size_t depth,
                            size_t *budget)
  {
    size_t i, n;
    mulib_json_type t = mulib_json_typeof (v);
    if (!*budget || depth > 32)
      return 0;
    --*budget;
    if (t == MULIB_JSON_STRING)
      {
        mulib_json_text (v, &n);
        if (n > *budget)
          return 0;
        *budget -= n;
      }
    if (t == MULIB_JSON_OBJECT || t == MULIB_JSON_ARRAY)
      {
        for (i = 0; i < mulib_json_size (v); ++i)
          {
            if (t == MULIB_JSON_OBJECT)
              {
                mulib_json_key (v, i, &n);
                if (n > *budget)
                  return 0;
                *budget -= n;
              }
            if (!mulib_browsertools_tree_ (mulib_json_at (v, i), depth + 1,
                                           budget))
              return 0;
          }
      }
    return 1;
  }
  static mulib_json_t *
  mulib_browsertools_clone_ (const mulib_json_t *v)
  {
    mulib_json_t *copy;
    size_t i, n;
    const char *s;
    mulib_json_type t = mulib_json_typeof (v);
    if (t == MULIB_JSON_STRING)
      {
        s = mulib_json_text (v, &n);
        return mulib_json_string_n (s, n);
      }
    if (t == MULIB_JSON_NUMBER)
      return mulib_json_number (mulib_json_numeric (v));
    if (t == MULIB_JSON_BOOL)
      return mulib_json_bool (mulib_json_boolean (v));
    copy = mulib_json_new (t);
    if (!copy)
      return NULL;
    for (i = 0; i < mulib_json_size (v); ++i)
      {
        mulib_json_t *child = mulib_browsertools_clone_ (mulib_json_at (v, i));
        int ok;
        if (!child)
          {
            mulib_json_destroy (copy);
            return NULL;
          }
        if (t == MULIB_JSON_ARRAY)
          ok = mulib_json_append (copy, child);
        else
          {
            s = mulib_json_key (v, i, &n);
            ok = mulib_json_set_n (copy, s, n, child);
          }
        if (!ok)
          {
            mulib_json_destroy (child);
            mulib_json_destroy (copy);
            return NULL;
          }
      }
    return copy;
  }
  mulib_browsertools_status_t
  mulib_browsertools_request (mulib_browsertools_t *c, const char *method,
                              const mulib_json_t *params, const char *session,
                              uint32_t *id)
  {
    mulib_json_t *o = NULL;
    char *wire = NULL, *saved = NULL;
    size_t n, budget;
    uint32_t request_id;
    mulib_browsertools_status_t rc = MULIB_BROWSERTOOLS_MEMORY;
    if (!c || !method || !*method || (session && !*session)
        || (params && mulib_json_typeof (params) != MULIB_JSON_OBJECT))
      return MULIB_BROWSERTOOLS_INVALID;
    if (!mulib_utf8_validate ((const unsigned char *)method, strlen (method))
        || (session
            && !mulib_utf8_validate ((const unsigned char *)session,
                                     strlen (session))))
      return MULIB_BROWSERTOOLS_INVALID;
    if (c->count == c->limits.pending || !c->next_id
        || strlen (method) > c->limits.message_bytes
        || (session && strlen (session) > c->limits.message_bytes))
      return MULIB_BROWSERTOOLS_LIMIT;
    budget = c->limits.message_bytes;
    if (params && !mulib_browsertools_tree_ (params, 0, &budget))
      return MULIB_BROWSERTOOLS_LIMIT;
    if (session)
      {
        n = strlen (session);
        if (n == SIZE_MAX)
          return MULIB_BROWSERTOOLS_LIMIT;
        saved = (char *)malloc (n + 1);
        if (!saved)
          goto done;
        memcpy (saved, session, n + 1);
      }
    o = mulib_json_new (MULIB_JSON_OBJECT);
    if (!o)
      goto done;
    request_id = c->next_id;
    if (!mulib_browsertools_set_ (o, "id", mulib_json_number (request_id))
        || !mulib_browsertools_set_ (o, "method", mulib_json_string (method))
        || (params
            && !mulib_browsertools_set_ (o, "params",
                                         mulib_browsertools_clone_ (params)))
        || (session
            && !mulib_browsertools_set_ (o, "sessionId",
                                         mulib_json_string (session))))
      goto done;
    wire = mulib_json_stringify (o, 0);
    if (!wire)
      goto done;
    n = strlen (wire);
    if (n > c->limits.message_bytes)
      {
        rc = MULIB_BROWSERTOOLS_LIMIT;
        goto done;
      }
    ++c->next_id; /* unsigned wrap to zero permanently exhausts IDs. */
    if (c->send (c->user, wire, n))
      {
        rc = MULIB_BROWSERTOOLS_TRANSPORT;
        goto done;
      }
    c->pending[c->count].id = request_id;
    c->pending[c->count++].session = saved;
    saved = NULL;
    if (id)
      *id = request_id;
    rc = MULIB_BROWSERTOOLS_OK;
  done:
    free (saved);
    free (wire);
    mulib_json_destroy (o);
    return rc;
  }
  static int
  mulib_browsertools_string_ (const mulib_json_t *v, int nonempty)
  {
    size_t n;
    const char *s = mulib_json_text (v, &n);
    return s && (!nonempty || n) && !memchr (s, 0, n);
  }
  static int
  mulib_browsertools_session_ (const char *s, const mulib_json_t *v)
  {
    size_t n;
    const char *text;
    if (!v)
      return !s;
    text = mulib_json_text (v, &n);
    return s && text && strlen (s) == n && !memcmp (s, text, n);
  }
  mulib_browsertools_status_t
  mulib_browsertools_receive (mulib_browsertools_t *c, const char *s, size_t n,
                              mulib_browsertools_message_t *out)
  {
    mulib_json_t *o, *id, *session, *method, *result, *error;
    double d;
    size_t i;
    mulib_browsertools_status_t rc;
    mulib_browsertools_message_t m;
    mulib_json_error_t diagnostic;
    if (!c || !out || out->json || (!s && n))
      return MULIB_BROWSERTOOLS_INVALID;
    if (n > c->limits.message_bytes)
      return MULIB_BROWSERTOOLS_LIMIT;
    o = mulib_json_parse (s, n, &diagnostic);
    if (!o)
      {
        if (diagnostic.message
            && (!strcmp (diagnostic.message, "out of memory")
                || !strcmp (diagnostic.message,
                            "capture allocation overflow")))
          return MULIB_BROWSERTOOLS_MEMORY;
        if (diagnostic.message && strstr (diagnostic.message, "limit"))
          return MULIB_BROWSERTOOLS_LIMIT;
        return MULIB_BROWSERTOOLS_INVALID;
      }
    if (mulib_json_typeof (o) != MULIB_JSON_OBJECT)
      goto invalid;
    id = mulib_json_get (o, "id");
    session = mulib_json_get (o, "sessionId");
    method = mulib_json_get (o, "method");
    result = mulib_json_get (o, "result");
    error = mulib_json_get (o, "error");
    if (session && !mulib_browsertools_string_ (session, 1))
      goto invalid;
    memset (&m, 0, sizeof m);
    m.json = o;
    if (!id)
      {
        mulib_json_t *params = mulib_json_get (o, "params");
        if (!mulib_browsertools_string_ (method, 1) || result || error
            || (params && mulib_json_typeof (params) != MULIB_JSON_OBJECT))
          goto invalid;
        rc = MULIB_BROWSERTOOLS_EVENT;
      }
    else
      {
        if (mulib_json_typeof (id) != MULIB_JSON_NUMBER || method
            || mulib_json_get (o, "params") || (!!result == !!error))
          goto invalid;
        d = mulib_json_numeric (id);
        if (d < 1 || d > UINT32_MAX || floor (d) != d)
          goto invalid;
        if (result && mulib_json_typeof (result) != MULIB_JSON_OBJECT)
          goto invalid;
        if (error)
          {
            mulib_json_t *code = mulib_json_get (error, "code");
            if (mulib_json_typeof (error) != MULIB_JSON_OBJECT
                || mulib_json_typeof (code) != MULIB_JSON_NUMBER
                || floor (mulib_json_numeric (code))
                       != mulib_json_numeric (code)
                || !mulib_browsertools_string_ (
                    mulib_json_get (error, "message"), 0))
              goto invalid;
          }
        m.id = (uint32_t)d;
        m.remote_error = !!error;
        rc = MULIB_BROWSERTOOLS_UNMATCHED;
        for (i = 0; i < c->count; ++i)
          if (c->pending[i].id == m.id
              && mulib_browsertools_session_ (c->pending[i].session, session))
            {
              free (c->pending[i].session);
              c->pending[i] = c->pending[--c->count];
              rc = MULIB_BROWSERTOOLS_RESPONSE;
              break;
            }
      }
    *out = m;
    return rc;
  invalid:
    mulib_json_destroy (o);
    return MULIB_BROWSERTOOLS_INVALID;
  }
  void
  mulib_browsertools_message_free (mulib_browsertools_message_t *m)
  {
    if (m)
      {
        mulib_json_destroy (m->json);
        memset (m, 0, sizeof *m);
      }
  }
  int
  mulib_browsertools_cancel (mulib_browsertools_t *c, uint32_t id)
  {
    size_t i;
    if (!c)
      return 0;
    for (i = 0; i < c->count; ++i)
      if (c->pending[i].id == id)
        {
          free (c->pending[i].session);
          c->pending[i] = c->pending[--c->count];
          return 1;
        }
    return 0;
  }
  size_t
  mulib_browsertools_pending (const mulib_browsertools_t *c)
  {
    return c ? c->count : 0;
  }
  static mulib_browsertools_status_t
  mulib_browsertools_command_ (mulib_browsertools_t *c, const char *method,
                               const char *key, const char *text,
                               const char *session, uint32_t *id, int evaluate)
  {
    mulib_json_t *p;
    mulib_browsertools_status_t rc;
    if (!c || !text || !*text)
      return MULIB_BROWSERTOOLS_INVALID;
    if (strlen (text) > c->limits.message_bytes)
      return MULIB_BROWSERTOOLS_LIMIT;
    if (!mulib_utf8_validate ((const unsigned char *)text, strlen (text)))
      return MULIB_BROWSERTOOLS_INVALID;
    p = mulib_json_new (MULIB_JSON_OBJECT);
    if (!p)
      return MULIB_BROWSERTOOLS_MEMORY;
    if (!mulib_browsertools_set_ (p, key, mulib_json_string (text))
        || (evaluate
            && (!mulib_browsertools_set_ (p, "returnByValue",
                                          mulib_json_bool (1))
                || !mulib_browsertools_set_ (p, "awaitPromise",
                                             mulib_json_bool (1)))))
      {
        mulib_json_destroy (p);
        return MULIB_BROWSERTOOLS_MEMORY;
      }
    rc = mulib_browsertools_request (c, method, p, session, id);
    mulib_json_destroy (p);
    return rc;
  }
  mulib_browsertools_status_t
  mulib_browsertools_navigate (mulib_browsertools_t *c, const char *url,
                               const char *session, uint32_t *id)
  {
    return mulib_browsertools_command_ (c, "Page.navigate", "url", url,
                                        session, id, 0);
  }
  mulib_browsertools_status_t
  mulib_browsertools_evaluate (mulib_browsertools_t *c, const char *expression,
                               const char *session, uint32_t *id)
  {
    return mulib_browsertools_command_ (c, "Runtime.evaluate", "expression",
                                        expression, session, id, 1);
  }
  mulib_json_t *
  mulib_browsertools_targets (const mulib_http_message_t *r,
                              const mulib_browsertools_limits_t *limits)
  {
    mulib_browsertools_limits_t l = mulib_browsertools_limits_ (limits);
    mulib_json_t *a;
    size_t i;
    if (!r || !r->is_response || r->status != 200
        || r->body_length > l.message_bytes || (!r->body && r->body_length))
      return NULL;
    a = mulib_json_parse ((const char *)r->body, r->body_length, NULL);
    if (mulib_json_typeof (a) != MULIB_JSON_ARRAY)
      {
        mulib_json_destroy (a);
        return NULL;
      }
    for (i = 0; i < mulib_json_size (a); ++i)
      {
        mulib_json_t *v = mulib_json_at (a, i);
        if (mulib_json_typeof (v) != MULIB_JSON_OBJECT
            || !mulib_browsertools_string_ (mulib_json_get (v, "id"), 1)
            || !mulib_browsertools_string_ (mulib_json_get (v, "type"), 1)
            || !mulib_browsertools_string_ (mulib_json_get (v, "url"), 0))
          {
            mulib_json_destroy (a);
            return NULL;
          }
      }
    return a;
  }
#ifdef __cplusplus
}
#endif
#endif
