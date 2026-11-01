/**
 * @file mulib-wayland.h
 * @brief Wayland native-endian frames and optional runtime-loaded client
 * connection. Define MULIB_IMPL_WAYLAND and MULIB_IMPL_DYNLOAD once; POSIX may
 * need -ldl.
 */
#ifndef MULIB_WAYLAND_H
#define MULIB_WAYLAND_H
#include "mulib-dynload.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_WAYLAND_OK = 1,
    MULIB_WAYLAND_MORE = 0,
    MULIB_WAYLAND_INVALID = -1,
    MULIB_WAYLAND_NOMEM = -2,
    MULIB_WAYLAND_EIO = -3,
    MULIB_WAYLAND_ENOTSUP = -4
  };
  /** @brief Borrowed frame payload; file-descriptor ancillary data is out of
   * scope. */
  typedef struct mulib_wayland_frame
  {
    uint32_t object;
    uint16_t opcode;
    const unsigned char *payload;
    size_t length;
  } mulib_wayland_frame_t;
  /** @brief Decode one native-endian, aligned-length frame; object zero is
   * invalid. On MORE/error output is unchanged, consumed zero. Input need not
   * be aligned.
   */
  int mulib_wayland_frame_parse (const void *bytes, size_t length,
                                 mulib_wayland_frame_t *out, size_t *consumed);
  /** @brief Allocate wire bytes; payload length must be a multiple of 4,
   * <=65524. Outputs unchanged on failure; free bytes with free(). This is not
   * socket sending.
   */
  int mulib_wayland_frame_encode (const mulib_wayland_frame_t *frame,
                                  unsigned char **bytes, size_t *length);
  /** @brief Opaque owning client connection, also owning its runtime library
   * handle. */
  typedef struct mulib_wayland_client mulib_wayland_client_t;
  /** @brief Load libwayland-client and connect to name (NULL = environment
   * default). NULL on missing runtime, connection or allocation failure; never
   * auto-launch a server.
   */
  mulib_wayland_client_t *mulib_wayland_connect (const char *name);
  /** @brief Disconnect/free; NULL safe. All proxies must be destroyed first.
   */
  void mulib_wayland_disconnect (mulib_wayland_client_t *client);
  /** @brief Borrow struct wl_display pointer, or NULL; valid until disconnect.
   */
  void *mulib_wayland_display (const mulib_wayland_client_t *client);
  /** @brief Borrow connection fd or -1; never close it directly. */
  int mulib_wayland_fd (const mulib_wayland_client_t *client);
  /** @brief Dispatch already queued events only; return native >=0 or -1. */
  int mulib_wayland_dispatch_pending (mulib_wayland_client_t *client);
  /** @brief Blocking native roundtrip; return native >=0 or -1. */
  int mulib_wayland_roundtrip (mulib_wayland_client_t *client);
  /** @brief Native nonblocking flush, >=0 or -1 (errno may be EAGAIN). */
  int mulib_wayland_flush (mulib_wayland_client_t *client);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAYLAND) && !defined(MULIB_WAYLAND_IMPLEMENTED)
#define MULIB_WAYLAND_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_wayland_frame_parse (const void *bytes, size_t n,
                             mulib_wayland_frame_t *out, size_t *used)
  {
    uint32_t header[2];
    size_t size;
    mulib_wayland_frame_t f;
    if (used)
      *used = 0;
    if ((!bytes && n) || !out)
      return MULIB_WAYLAND_INVALID;
    if (n < 8)
      return MULIB_WAYLAND_MORE;
    memcpy (header, bytes, 8);
    size = header[1] >> 16;
    if (!header[0] || size < 8 || size % 4)
      return MULIB_WAYLAND_INVALID;
    if (n < size)
      return MULIB_WAYLAND_MORE;
    f.object = header[0];
    f.opcode = (uint16_t)header[1];
    f.payload = (const unsigned char *)bytes + 8;
    f.length = size - 8;
    *out = f;
    if (used)
      *used = size;
    return MULIB_WAYLAND_OK;
  }
  int
  mulib_wayland_frame_encode (const mulib_wayland_frame_t *f,
                              unsigned char **bytes, size_t *length)
  {
    unsigned char *wire;
    uint32_t header[2];
    if (!f || !bytes || !length || !f->object || (!f->payload && f->length)
        || f->length % 4 || f->length > 65524)
      return MULIB_WAYLAND_INVALID;
    wire = (unsigned char *)malloc (f->length + 8);
    if (!wire)
      return MULIB_WAYLAND_NOMEM;
    header[0] = f->object;
    header[1] = (uint32_t)(f->length + 8) << 16 | f->opcode;
    memcpy (wire, header, 8);
    if (f->length)
      memcpy (wire + 8, f->payload, f->length);
    *bytes = wire;
    *length = f->length + 8;
    return MULIB_WAYLAND_OK;
  }
  struct wl_display;
  struct mulib_wayland_client
  {
    mulib_dynload_t library;
    struct wl_display *display;
    struct wl_display *(*connect) (const char *);
    void (*disconnect) (struct wl_display *);
    int (*get_fd) (struct wl_display *), (*pending) (struct wl_display *),
        (*roundtrip) (struct wl_display *), (*flush) (struct wl_display *);
  };
  static int
  mulib_wayland_symbol_ (mulib_dynload_t *lib, const char *name, void *out,
                         size_t size)
  {
    void *symbol = NULL;
    if (size != sizeof (symbol)
        || mulib_dynload_symbol (lib, name, &symbol, NULL, 0) || !symbol)
      return 0;
    memcpy (out, &symbol, size);
    return 1;
  }
  mulib_wayland_client_t *
  mulib_wayland_connect (const char *name)
  {
    mulib_wayland_client_t *c
        = (mulib_wayland_client_t *)calloc (1, sizeof (*c));
    if (!c)
      return NULL;
#ifdef __APPLE__
    if (mulib_dynload_open (&c->library, "libwayland-client.dylib", NULL, 0))
      goto fail;
#else
  if (mulib_dynload_open (&c->library, "libwayland-client.so.0", NULL, 0))
    goto fail;
#endif
    if (!mulib_wayland_symbol_ (&c->library, "wl_display_connect", &c->connect,
                                sizeof (c->connect))
        || !mulib_wayland_symbol_ (&c->library, "wl_display_disconnect",
                                   &c->disconnect, sizeof (c->disconnect))
        || !mulib_wayland_symbol_ (&c->library, "wl_display_get_fd",
                                   &c->get_fd, sizeof (c->get_fd))
        || !mulib_wayland_symbol_ (&c->library, "wl_display_dispatch_pending",
                                   &c->pending, sizeof (c->pending))
        || !mulib_wayland_symbol_ (&c->library, "wl_display_roundtrip",
                                   &c->roundtrip, sizeof (c->roundtrip))
        || !mulib_wayland_symbol_ (&c->library, "wl_display_flush", &c->flush,
                                   sizeof (c->flush)))
      goto fail;
    c->display = c->connect (name);
    if (!c->display)
      goto fail;
    return c;
  fail:
    (void)mulib_dynload_close (&c->library, NULL, 0);
    free (c);
    return NULL;
  }
  void
  mulib_wayland_disconnect (mulib_wayland_client_t *c)
  {
    if (!c)
      return;
    c->disconnect (c->display);
    (void)mulib_dynload_close (&c->library, NULL, 0);
    free (c);
  }
  void *
  mulib_wayland_display (const mulib_wayland_client_t *c)
  {
    return c ? c->display : NULL;
  }
  int
  mulib_wayland_fd (const mulib_wayland_client_t *c)
  {
    return c ? c->get_fd (c->display) : -1;
  }
  int
  mulib_wayland_dispatch_pending (mulib_wayland_client_t *c)
  {
    return c ? c->pending (c->display) : -1;
  }
  int
  mulib_wayland_roundtrip (mulib_wayland_client_t *c)
  {
    return c ? c->roundtrip (c->display) : -1;
  }
  int
  mulib_wayland_flush (mulib_wayland_client_t *c)
  {
    return c ? c->flush (c->display) : -1;
  }
#ifdef __cplusplus
}
#endif
#endif
