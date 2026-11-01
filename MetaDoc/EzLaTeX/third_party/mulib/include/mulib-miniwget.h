/** @file mulib-miniwget.h
 * @brief Bounded binary GET download to caller-owned memory, using webclient.
 */
#ifndef MULIB_MINIWGET_H
#define MULIB_MINIWGET_H
#include "mulib-webclient.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Download result; STATUS means the final HTTP status was not 2xx.
   */
  typedef enum mulib_miniwget_result
  {
    MULIB_MINIWGET_OK = 1,
    MULIB_MINIWGET_INVALID = -1,
    MULIB_MINIWGET_LIMIT = -2,
    MULIB_MINIWGET_NOMEM = -3,
    MULIB_MINIWGET_TRANSPORT = -4,
    MULIB_MINIWGET_STATUS = -5
  } mulib_miniwget_result;
  /** @brief GET into a borrowed destination, never adding a terminator.
   * On success set length; on error destination and length remain unchanged.
   * NULL destination is valid only at zero capacity. No redirects or file
   * writes.
   */
  mulib_miniwget_result
  mulib_miniwget_download (const char *url,
                           const mulib_webclient_options_t *options,
                           mulib_webclient_transport_fn transport, void *user,
                           void *destination, size_t capacity, size_t *length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MINIWGET) && !defined(MULIB_MINIWGET_IMPLEMENTED)
#define MULIB_MINIWGET_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_miniwget_result
  mulib_miniwget_download (const char *url,
                           const mulib_webclient_options_t *options,
                           mulib_webclient_transport_fn transport, void *user,
                           void *destination, size_t capacity, size_t *length)
  {
    mulib_webclient_options_t limits = { { 0, 0, 0, 0 }, 0 };
    mulib_http_message_t response;
    mulib_webclient_result rc;
    size_t bound = capacity ? capacity : 1;
    if ((!destination && capacity) || !length)
      return MULIB_MINIWGET_INVALID;
    if (options)
      limits = *options;
    if (!limits.http.max_body_bytes || limits.http.max_body_bytes > bound)
      limits.http.max_body_bytes = bound;
    memset (&response, 0, sizeof (response));
    rc = mulib_webclient_request (url, "GET", NULL, 0, NULL, 0, &limits,
                                  transport, user, &response);
    if (rc != MULIB_WEBCLIENT_OK)
      return (mulib_miniwget_result)rc;
    if (response.status < 200 || response.status >= 300)
      {
        mulib_http_message_free (&response);
        return MULIB_MINIWGET_STATUS;
      }
    if (response.body_length > capacity)
      {
        mulib_http_message_free (&response);
        return MULIB_MINIWGET_LIMIT;
      }
    if (response.body_length)
      memcpy (destination, response.body, response.body_length);
    *length = response.body_length;
    mulib_http_message_free (&response);
    return MULIB_MINIWGET_OK;
  }
#ifdef __cplusplus
}
#endif
#endif
