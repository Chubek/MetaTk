/**
 * @file mulib-rpc.h
 * @brief Synchronous transport-independent JSON-RPC 2.0 server dispatch.
 * Define MULIB_IMPL_RPC once. JSON, PARSEC, STRBUF and UTF8 implementations
 * must be supplied independently. No network, global registry, or external
 * libs.
 */
#ifndef MULIB_RPC_H
#define MULIB_RPC_H
#include "mulib-json.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Callback borrows params (NULL if absent), owns *result initially
   * NULL. Return 0 for success; otherwise return a JSON-RPC integer error code
   * and optionally supply error data in *result. Dispatcher always takes
   * *result. Successful NULL results become JSON null. Never return borrowed
   * JSON values.
   */
  typedef int (*mulib_rpc_handler_t) (const mulib_json_t *params,
                                      mulib_json_t **result, void *user);
  /** @brief Borrowed method registration; names must be unique and not start
   * rpc. */
  typedef struct mulib_rpc_method
  {
    const char *name;
    mulib_rpc_handler_t handler;
    void *user;
  } mulib_rpc_method_t;
  /** @brief Dispatch a borrowed DOM request (or batch).
   * Return 1 and an owned response, or NULL response for notifications only.
   * Return 0 on allocation failure or invalid registry; output is unchanged.
   * max_batch == 0 selects 64; oversized batches yield -32000 without invoking
   * handlers.
   */
  int mulib_rpc_dispatch (const mulib_rpc_method_t *methods,
                          size_t method_count, const mulib_json_t *request,
                          size_t max_batch, mulib_json_t **response);
  /** @brief Parse exact UTF-8 bytes, dispatch, and stringify to malloc-owned
   * text. Malformed/trailing input yields a -32700 response; notifications
   * yield NULL. Return 0 only for local failure (including an invalid
   * registry).
   */
  int mulib_rpc_process (const mulib_rpc_method_t *methods,
                         size_t method_count, const char *source,
                         size_t length, size_t max_batch, char **response);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_RPC_H */

#if defined(MULIB_IMPL_RPC) && !defined(MULIB_RPC_IMPLEMENTED)
#define MULIB_RPC_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_rpc_set_ (mulib_json_t *o, const char *key, mulib_json_t *v)
  {
    if (!v)
      return 0;
    if (mulib_json_set (o, key, v))
      return 1;
    mulib_json_destroy (v);
    return 0;
  }
  static mulib_json_t *
  mulib_rpc_id_ (const mulib_json_t *id)
  {
    size_t n;
    const char *text;
    switch (mulib_json_typeof (id))
      {
      case MULIB_JSON_STRING:
        text = mulib_json_text (id, &n);
        return mulib_json_string_n (text, n);
      case MULIB_JSON_NUMBER:
        return mulib_json_number (mulib_json_numeric (id));
      default:
        return mulib_json_new (MULIB_JSON_NULL);
      }
  }
  static const char *
  mulib_rpc_error_text_ (int code)
  {
    switch (code)
      {
      case -32700:
        return "Parse error";
      case -32600:
        return "Invalid Request";
      case -32601:
        return "Method not found";
      case -32602:
        return "Invalid params";
      case -32603:
        return "Internal error";
      default:
        return "Server error";
      }
  }
  /* Takes payload on both success and failure. */
  static mulib_json_t *
  mulib_rpc_response_ (const mulib_json_t *id, int code, mulib_json_t *payload,
                       const char *message)
  {
    mulib_json_t *o = mulib_json_new (MULIB_JSON_OBJECT), *error = NULL;
    if (!o || !mulib_rpc_set_ (o, "jsonrpc", mulib_json_string ("2.0"))
        || !mulib_rpc_set_ (o, "id", mulib_rpc_id_ (id)))
      goto fail;
    if (code)
      {
        error = mulib_json_new (MULIB_JSON_OBJECT);
        if (!error
            || !mulib_rpc_set_ (error, "code",
                                mulib_json_number ((double)code))
            || !mulib_rpc_set_ (
                error, "message",
                mulib_json_string (message ? message
                                           : mulib_rpc_error_text_ (code))))
          goto fail;
        if (payload)
          {
            if (!mulib_json_set (error, "data", payload))
              goto fail;
            payload = NULL;
          }
        if (!mulib_json_set (o, "error", error))
          goto fail;
        error = NULL;
      }
    else
      {
        if (!payload)
          payload = mulib_json_new (MULIB_JSON_NULL);
        if (!payload || !mulib_json_set (o, "result", payload))
          goto fail;
        payload = NULL;
      }
    return o;
  fail:
    mulib_json_destroy (error);
    mulib_json_destroy (payload);
    mulib_json_destroy (o);
    return NULL;
  }
  static int
  mulib_rpc_registry_ (const mulib_rpc_method_t *methods, size_t count)
  {
    if (!methods && count)
      return 0;
    for (size_t i = 0; i < count; ++i)
      {
        if (!methods[i].name || !methods[i].handler
            || !strncmp (methods[i].name, "rpc.", 4))
          return 0;
        for (size_t j = 0; j < i; ++j)
          if (!strcmp (methods[i].name, methods[j].name))
            return 0;
      }
    return 1;
  }
  static int
  mulib_rpc_one_ (const mulib_rpc_method_t *methods, size_t count,
                  const mulib_json_t *req, mulib_json_t **response)
  {
    const mulib_json_t *version, *method, *params, *id;
    const char *name, *vtext;
    size_t name_len = 0, vlen = 0;
    mulib_json_t *result = NULL;
    int code = -32601;
    *response = NULL;
    if (mulib_json_typeof (req) != MULIB_JSON_OBJECT)
      goto invalid;
    version = mulib_json_get (req, "jsonrpc");
    method = mulib_json_get (req, "method");
    params = mulib_json_get (req, "params");
    id = mulib_json_get (req, "id");
    vtext = mulib_json_text (version, &vlen);
    name = mulib_json_text (method, &name_len);
    if (!vtext || vlen != 3 || memcmp (vtext, "2.0", 3) || !name
        || (params && mulib_json_typeof (params) != MULIB_JSON_ARRAY
            && mulib_json_typeof (params) != MULIB_JSON_OBJECT)
        || (id && mulib_json_typeof (id) != MULIB_JSON_NULL
            && mulib_json_typeof (id) != MULIB_JSON_NUMBER
            && mulib_json_typeof (id) != MULIB_JSON_STRING))
      goto invalid;
    /* Reject duplicate protocol members if the DOM exposes them, in linear
     * time. */
    {
      static const char *const keys[]
          = { "jsonrpc", "method", "params", "id" };
      unsigned seen = 0;
      for (size_t i = 0; i < mulib_json_size (req); ++i)
        {
          size_t n;
          const char *key = mulib_json_key (req, i, &n);
          for (unsigned j = 0; j < 4; ++j)
            if (n == strlen (keys[j]) && !memcmp (key, keys[j], n))
              {
                if (seen & (1u << j))
                  goto invalid;
                seen |= 1u << j;
              }
        }
    }
    for (size_t i = 0; i < count; ++i)
      {
        if (strlen (methods[i].name) == name_len
            && !memcmp (methods[i].name, name, name_len))
          {
            code = methods[i].handler (params, &result, methods[i].user);
            break;
          }
      }
    if (!id)
      {
        mulib_json_destroy (result);
        return 1;
      }
    *response = mulib_rpc_response_ (id, code, result, NULL);
    return *response != NULL;
  invalid:
    *response = mulib_rpc_response_ (NULL, -32600, NULL, NULL);
    return *response != NULL;
  }
  int
  mulib_rpc_dispatch (const mulib_rpc_method_t *methods, size_t method_count,
                      const mulib_json_t *request, size_t max_batch,
                      mulib_json_t **response)
  {
    mulib_json_t *out = NULL;
    if (!response || !mulib_rpc_registry_ (methods, method_count))
      return 0;
    if (mulib_json_typeof (request) != MULIB_JSON_ARRAY)
      {
        if (!mulib_rpc_one_ (methods, method_count, request, &out))
          return 0;
      }
    else
      {
        size_t count = mulib_json_size (request);
        if (!max_batch)
          max_batch = 64;
        if (!count)
          out = mulib_rpc_response_ (NULL, -32600, NULL, NULL);
        else if (count > max_batch)
          out = mulib_rpc_response_ (NULL, -32000, NULL,
                                     "Batch limit exceeded");
        else
          {
            out = mulib_json_new (MULIB_JSON_ARRAY);
            if (!out)
              return 0;
            for (size_t i = 0; i < count; ++i)
              {
                mulib_json_t *one = NULL;
                if (!mulib_rpc_one_ (methods, method_count,
                                     mulib_json_at (request, i), &one))
                  {
                    mulib_json_destroy (out);
                    return 0;
                  }
                if (one && !mulib_json_append (out, one))
                  {
                    mulib_json_destroy (one);
                    mulib_json_destroy (out);
                    return 0;
                  }
              }
            if (!mulib_json_size (out))
              {
                mulib_json_destroy (out);
                *response = NULL;
                return 1;
              }
          }
        if (!out)
          return 0;
      }
    *response = out;
    return 1;
  }
  int
  mulib_rpc_process (const mulib_rpc_method_t *methods, size_t method_count,
                     const char *source, size_t length, size_t max_batch,
                     char **response)
  {
    mulib_json_t *req, *res = NULL;
    char *text;
    if (!response || (!source && length)
        || !mulib_rpc_registry_ (methods, method_count))
      return 0;
    req = mulib_json_parse (source ? source : "", length, NULL);
    if (!req)
      res = mulib_rpc_response_ (NULL, -32700, NULL, NULL);
    else
      {
        int ok
            = mulib_rpc_dispatch (methods, method_count, req, max_batch, &res);
        mulib_json_destroy (req);
        if (!ok)
          return 0;
        if (!res)
          {
            *response = NULL;
            return 1;
          }
      }
    if (!res)
      return 0;
    text = mulib_json_stringify (res, 0);
    mulib_json_destroy (res);
    if (!text)
      return 0;
    *response = text;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
