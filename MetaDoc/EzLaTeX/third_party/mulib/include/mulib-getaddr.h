/**
 * @file mulib-getaddr.h
 * @brief Portable literal addresses and optional native system host lookup.
 * Define MULIB_IMPL_GETADDR, MULIB_IMPL_INET6, MULIB_IMPL_INET4 and
 * MULIB_IMPL_PARSEC separately. System lookup has no DNS-stack dependency.
 */
#ifndef MULIB_GETADDR_H
#define MULIB_GETADDR_H
#include "mulib-inet6.h"
/* POSIX feature-test macros must be selected by the application, before
 * headers. */
#ifndef MULIB_GETADDR_HAS_SYSTEM
#if !defined(MULIB_GETADDR_NO_SYSTEM)                                         \
    && (defined(_WIN32) || defined(__APPLE__)                                 \
        || ((defined(__unix__) || defined(__linux__))                         \
            && (defined(_GNU_SOURCE) || defined(_DEFAULT_SOURCE)              \
                || (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L)   \
                || (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 600))))
#define MULIB_GETADDR_HAS_SYSTEM 1
#else
#define MULIB_GETADDR_HAS_SYSTEM 0
#endif
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_GETADDR_OK = 1,
    MULIB_GETADDR_INVALID = -1,
    MULIB_GETADDR_LIMIT = -2,
    MULIB_GETADDR_UNSUPPORTED = -3,
    MULIB_GETADDR_NOTFOUND = -4,
    MULIB_GETADDR_SYSTEM_ERROR = -5,
    MULIB_GETADDR_SYSTEM = 1
  };
  /** @brief Owned address value, family 4/6, network-order bytes, host-order
   * port and IPv6 scope. */
  typedef struct mulib_getaddr_address
  {
    unsigned family;
    unsigned char bytes[16];
    uint16_t port;
    uint32_t scope_id;
  } mulib_getaddr_address_t;
  /**
   * @brief Resolve a NUL-terminated literal or (with SYSTEM flag) a system
   * host name. Family is 0/4/6; no service strings, zones, brackets,
   * null/passive host or flags other than SYSTEM. Count reports required
   * distinct addresses on LIMIT; output is then unchanged. Native lookup is
   * synchronous and can allocate internally; native lists longer than 1024
   * nodes return LIMIT without publishing count.
   */
  int mulib_getaddr_resolve (const char *host, uint16_t port, unsigned family,
                             unsigned flags,
                             mulib_getaddr_address_t *addresses,
                             size_t capacity, size_t *count);
  /** @brief Format numeric bytes only (no port/scope); unchanged output on
   * failure. */
  int mulib_getaddr_format (const mulib_getaddr_address_t *address, char *out,
                            size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_GETADDR) && !defined(MULIB_GETADDR_IMPLEMENTED)
#define MULIB_GETADDR_IMPLEMENTED
#include <string.h>
#if MULIB_GETADDR_HAS_SYSTEM
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif
#endif
#ifdef __cplusplus
extern "C"
{
#endif
#if MULIB_GETADDR_HAS_SYSTEM
  static int
  mulib_getaddr_native_ (const struct addrinfo *ai, uint16_t port,
                         mulib_getaddr_address_t *out)
  {
    memset (out, 0, sizeof (*out));
    out->port = port;
    if (ai->ai_family == AF_INET && ai->ai_addr
        && ai->ai_addrlen >= sizeof (struct sockaddr_in))
      {
        const struct sockaddr_in *a = (const struct sockaddr_in *)ai->ai_addr;
        out->family = 4;
        memcpy (out->bytes, &a->sin_addr, 4);
        return 1;
      }
    if (ai->ai_family == AF_INET6 && ai->ai_addr
        && ai->ai_addrlen >= sizeof (struct sockaddr_in6))
      {
        const struct sockaddr_in6 *a
            = (const struct sockaddr_in6 *)ai->ai_addr;
        out->family = 6;
        memcpy (out->bytes, &a->sin6_addr, 16);
        out->scope_id = a->sin6_scope_id;
        return 1;
      }
    return 0;
  }
  static int
  mulib_getaddr_duplicate_ (const struct addrinfo *head,
                            const struct addrinfo *current, uint16_t port)
  {
    mulib_getaddr_address_t a, b;
    if (!mulib_getaddr_native_ (current, port, &a))
      return 1;
    for (const struct addrinfo *p = head; p != current; p = p->ai_next)
      {
        if (mulib_getaddr_native_ (p, port, &b) && a.family == b.family
            && a.scope_id == b.scope_id && !memcmp (a.bytes, b.bytes, 16))
          return 1;
      }
    return 0;
  }
#endif
  int
  mulib_getaddr_resolve (const char *host, uint16_t port, unsigned family,
                         unsigned flags, mulib_getaddr_address_t *out,
                         size_t cap, size_t *count)
  {
    mulib_getaddr_address_t a;
    mulib_inet4_t a4;
    mulib_inet6_t a6;
    size_t n = 0;
    int literal = 0;
    if (!host || !count || (cap && !out)
        || (family != 0 && family != 4 && family != 6) || (flags & ~1u))
      return MULIB_GETADDR_INVALID;
    while (n < 254 && host[n])
      ++n;
    if (!n || n == 254)
      return MULIB_GETADDR_INVALID;
    memset (&a, 0, sizeof (a));
    a.port = port;
    if (mulib_inet4_parse (host, n, &a4))
      {
        a.family = 4;
        memcpy (a.bytes, a4.bytes, 4);
        literal = 1;
      }
    else if (mulib_inet6_parse (host, n, &a6))
      {
        a.family = 6;
        memcpy (a.bytes, a6.bytes, 16);
        literal = 1;
      }
    if (literal)
      {
        if (family && a.family != family)
          return MULIB_GETADDR_NOTFOUND;
        if (!cap)
          {
            *count = 1;
            return MULIB_GETADDR_LIMIT;
          }
        *out = a;
        *count = 1;
        return MULIB_GETADDR_OK;
      }
    /* Do not let permissive native parsers reinterpret malformed numeric
     * forms. */
    {
      int numeric = 1;
      size_t label = 0;
      for (size_t i = 0; i < n; ++i)
        {
          unsigned char c = (unsigned char)host[i];
          if (c == ':' || c == '[' || c == ']' || c == '%' || c <= 32
              || c >= 127)
            return MULIB_GETADDR_INVALID;
          if (!(c >= '0' && c <= '9') && c != '.')
            numeric = 0;
          if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_'))
            return MULIB_GETADDR_INVALID;
          if (c == '.')
            {
              if (!label)
                return MULIB_GETADDR_INVALID;
              label = 0;
            }
          else if (++label > 63)
            return MULIB_GETADDR_INVALID;
        }
      if (numeric)
        return MULIB_GETADDR_INVALID;
    }
    if (!(flags & MULIB_GETADDR_SYSTEM))
      return MULIB_GETADDR_NOTFOUND;
#if MULIB_GETADDR_HAS_SYSTEM
    {
      struct addrinfo hints, *head = NULL;
      size_t required = 0, index = 0, nodes = 0;
      int rc;
      memset (&hints, 0, sizeof (hints));
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_flags = AI_NUMERICHOST;
      /* Reject legacy numeric syntax accepted by some native resolvers. */
      rc = getaddrinfo (host, NULL, &hints, &head);
      if (!rc)
        {
          freeaddrinfo (head);
          return MULIB_GETADDR_INVALID;
        }
      if (rc == EAI_MEMORY)
        return MULIB_GETADDR_SYSTEM_ERROR;
      head = NULL;
      hints.ai_flags = 0;
      hints.ai_family = family == 4   ? AF_INET
                        : family == 6 ? AF_INET6
                                      : AF_UNSPEC;
      rc = getaddrinfo (host, NULL, &hints, &head);
      if (rc)
        return rc == EAI_NONAME ? MULIB_GETADDR_NOTFOUND
                                : MULIB_GETADDR_SYSTEM_ERROR;
      for (const struct addrinfo *p = head; p; p = p->ai_next)
        {
          if (++nodes > 1024)
            {
              freeaddrinfo (head);
              return MULIB_GETADDR_LIMIT;
            }
          if (!mulib_getaddr_duplicate_ (head, p, port))
            ++required;
        }
      if (!required)
        {
          freeaddrinfo (head);
          return MULIB_GETADDR_NOTFOUND;
        }
      if (required > cap)
        {
          freeaddrinfo (head);
          *count = required;
          return MULIB_GETADDR_LIMIT;
        }
      for (const struct addrinfo *p = head; p; p = p->ai_next)
        if (!mulib_getaddr_duplicate_ (head, p, port))
          {
            (void)mulib_getaddr_native_ (p, port, &out[index++]);
          }
      freeaddrinfo (head);
      *count = required;
      return MULIB_GETADDR_OK;
    }
#else
  return MULIB_GETADDR_UNSUPPORTED;
#endif
  }
  int
  mulib_getaddr_format (const mulib_getaddr_address_t *a, char *out,
                        size_t cap)
  {
    if (!a || !out)
      return MULIB_GETADDR_INVALID;
    if (a->family == 4)
      {
        mulib_inet4_t value;
        memcpy (value.bytes, a->bytes, 4);
        return mulib_inet4_format (&value, out, cap) ? MULIB_GETADDR_OK
                                                     : MULIB_GETADDR_LIMIT;
      }
    if (a->family == 6)
      {
        mulib_inet6_t value;
        memcpy (value.bytes, a->bytes, 16);
        return mulib_inet6_format (&value, out, cap) ? MULIB_GETADDR_OK
                                                     : MULIB_GETADDR_LIMIT;
      }
    return MULIB_GETADDR_INVALID;
  }
#ifdef __cplusplus
}
#endif
#endif
