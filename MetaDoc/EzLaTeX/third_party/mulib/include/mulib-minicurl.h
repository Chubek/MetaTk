/** @file mulib-minicurl.h
 * @brief Small GET/body-request convenience API built on mulib-webclient.
 */
#ifndef MULIB_MINICURL_H
#define MULIB_MINICURL_H
#include "mulib-webclient.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief GET an http URL. Response ownership and limits match webclient. */
  mulib_webclient_result
  mulib_minicurl_get (const char *url,
                      const mulib_webclient_options_t *options,
                      mulib_webclient_transport_fn transport, void *user,
                      mulib_http_message_t *response);
  /** @brief Send a method and binary body with an optional Content-Type.
   * All inputs are borrowed. Any HTTP status is a successful transaction.
   */
  mulib_webclient_result
  mulib_minicurl_send (const char *url, const char *method,
                       const char *content_type, const void *body,
                       size_t length, const mulib_webclient_options_t *options,
                       mulib_webclient_transport_fn transport, void *user,
                       mulib_http_message_t *response);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MINICURL) && !defined(MULIB_MINICURL_IMPLEMENTED)
#define MULIB_MINICURL_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_webclient_result
  mulib_minicurl_get (const char *url,
                      const mulib_webclient_options_t *options,
                      mulib_webclient_transport_fn transport, void *user,
                      mulib_http_message_t *response)
  {
    return mulib_webclient_request (url, "GET", NULL, 0, NULL, 0, options,
                                    transport, user, response);
  }
  mulib_webclient_result
  mulib_minicurl_send (const char *url, const char *method,
                       const char *content_type, const void *body,
                       size_t length, const mulib_webclient_options_t *options,
                       mulib_webclient_transport_fn transport, void *user,
                       mulib_http_message_t *response)
  {
    mulib_http_header_t header;
    header.name = (char *)"Content-Type";
    header.value = (char *)content_type;
    return mulib_webclient_request (url, method, content_type ? &header : NULL,
                                    content_type ? 1 : 0, body, length,
                                    options, transport, user, response);
  }
#ifdef __cplusplus
}
#endif
#endif
