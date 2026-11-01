/**
 * @file mulib-xlib.h
 * @brief Optional runtime-loaded Xlib display and simple-window operations.
 * Define MULIB_IMPL_XLIB and MULIB_IMPL_DYNLOAD once; POSIX may need -ldl.
 */
#ifndef MULIB_XLIB_H
#define MULIB_XLIB_H
#include "mulib-dynload.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Opaque owning display connection and library handle;
   * single-threaded API. */
  typedef struct mulib_xlib mulib_xlib_t;
  /** @brief Native X11 Window XID, borrowed from its display. */
  typedef unsigned long mulib_xlib_window_t;
  /** @brief Load libX11 and open name (NULL = DISPLAY default); NULL on
   * failure. */
  mulib_xlib_t *mulib_xlib_open (const char *name);
  /** @brief Close display, destroy all its windows, unload runtime; NULL safe.
   */
  void mulib_xlib_close (mulib_xlib_t *client);
  /** @brief Borrow Display pointer until close, for native Xlib
   * interoperability. */
  void *mulib_xlib_display (const mulib_xlib_t *client);
  /** @brief Query default-screen dimensions; 1 success, 0 invalid. */
  int mulib_xlib_size (const mulib_xlib_t *client, unsigned *width,
                       unsigned *height);
  /** @brief Queue simple black-background window creation on default root;
   * zero on local invalid arguments. X11 errors are asynchronous and use
   * Xlib's handler. Dimensions must be 1..65535 and coordinates -32768..32767
   * (X11 wire bounds).
   */
  mulib_xlib_window_t mulib_xlib_window_create (mulib_xlib_t *client, int x,
                                                int y, unsigned width,
                                                unsigned height);
  /** @brief Queue title change; 1 queued, 0 invalid. Xlib copies the borrowed
   * title. */
  int mulib_xlib_window_title (mulib_xlib_t *client,
                               mulib_xlib_window_t window, const char *title);
  /** @brief Queue map request; 1 queued, 0 invalid. Does not flush. */
  int mulib_xlib_window_map (mulib_xlib_t *client, mulib_xlib_window_t window);
  /** @brief Queue destruction; window becomes unusable. 1 queued, 0 invalid.
   */
  int mulib_xlib_window_destroy (mulib_xlib_t *client,
                                 mulib_xlib_window_t window);
  /** @brief Flush requests; 1 flushed, 0 invalid. Native I/O errors use Xlib
   * policy. */
  int mulib_xlib_flush (mulib_xlib_t *client);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_XLIB) && !defined(MULIB_XLIB_IMPLEMENTED)
#define MULIB_XLIB_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  struct _XDisplay;
  struct mulib_xlib
  {
    mulib_dynload_t library;
    struct _XDisplay *display;
    struct _XDisplay *(*open) (const char *);
    int (*close) (struct _XDisplay *);
    int (*screen) (struct _XDisplay *), (*width) (struct _XDisplay *, int),
        (*height) (struct _XDisplay *, int);
    unsigned long (*root) (struct _XDisplay *, int);
    unsigned long (*create) (struct _XDisplay *, unsigned long, int, int,
                             unsigned, unsigned, unsigned, unsigned long,
                             unsigned long);
    int (*title) (struct _XDisplay *, unsigned long, const char *);
    int (*map) (struct _XDisplay *, unsigned long),
        (*destroy) (struct _XDisplay *, unsigned long),
        (*flush) (struct _XDisplay *);
  };
  static int
  mulib_xlib_symbol_ (mulib_dynload_t *lib, const char *name, void *out,
                      size_t size)
  {
    void *symbol = NULL;
    if (size != sizeof (symbol)
        || mulib_dynload_symbol (lib, name, &symbol, NULL, 0) || !symbol)
      return 0;
    memcpy (out, &symbol, size);
    return 1;
  }
  mulib_xlib_t *
  mulib_xlib_open (const char *name)
  {
    mulib_xlib_t *c = (mulib_xlib_t *)calloc (1, sizeof (*c));
    if (!c)
      return NULL;
#ifdef __APPLE__
    if (mulib_dynload_open (&c->library, "libX11.dylib", NULL, 0))
      goto fail;
#else
  if (mulib_dynload_open (&c->library, "libX11.so.6", NULL, 0))
    goto fail;
#endif
    if (!mulib_xlib_symbol_ (&c->library, "XOpenDisplay", &c->open,
                             sizeof (c->open))
        || !mulib_xlib_symbol_ (&c->library, "XCloseDisplay", &c->close,
                                sizeof (c->close))
        || !mulib_xlib_symbol_ (&c->library, "XDefaultScreen", &c->screen,
                                sizeof (c->screen))
        || !mulib_xlib_symbol_ (&c->library, "XDisplayWidth", &c->width,
                                sizeof (c->width))
        || !mulib_xlib_symbol_ (&c->library, "XDisplayHeight", &c->height,
                                sizeof (c->height))
        || !mulib_xlib_symbol_ (&c->library, "XRootWindow", &c->root,
                                sizeof (c->root))
        || !mulib_xlib_symbol_ (&c->library, "XCreateSimpleWindow", &c->create,
                                sizeof (c->create))
        || !mulib_xlib_symbol_ (&c->library, "XStoreName", &c->title,
                                sizeof (c->title))
        || !mulib_xlib_symbol_ (&c->library, "XMapWindow", &c->map,
                                sizeof (c->map))
        || !mulib_xlib_symbol_ (&c->library, "XDestroyWindow", &c->destroy,
                                sizeof (c->destroy))
        || !mulib_xlib_symbol_ (&c->library, "XFlush", &c->flush,
                                sizeof (c->flush)))
      goto fail;
    c->display = c->open (name);
    if (!c->display)
      goto fail;
    return c;
  fail:
    (void)mulib_dynload_close (&c->library, NULL, 0);
    free (c);
    return NULL;
  }
  void
  mulib_xlib_close (mulib_xlib_t *c)
  {
    if (!c)
      return;
    (void)c->close (c->display);
    (void)mulib_dynload_close (&c->library, NULL, 0);
    free (c);
  }
  void *
  mulib_xlib_display (const mulib_xlib_t *c)
  {
    return c ? c->display : NULL;
  }
  int
  mulib_xlib_size (const mulib_xlib_t *c, unsigned *w, unsigned *h)
  {
    int screen, width, height;
    if (!c || !w || !h)
      return 0;
    screen = c->screen (c->display);
    width = c->width (c->display, screen);
    height = c->height (c->display, screen);
    if (width <= 0 || height <= 0)
      return 0;
    *w = (unsigned)width;
    *h = (unsigned)height;
    return 1;
  }
  mulib_xlib_window_t
  mulib_xlib_window_create (mulib_xlib_t *c, int x, int y, unsigned w,
                            unsigned h)
  {
    if (!c || !w || !h || w > 65535 || h > 65535 || x < -32768 || x > 32767
        || y < -32768 || y > 32767)
      return 0;
    return c->create (c->display, c->root (c->display, c->screen (c->display)),
                      x, y, w, h, 0, 0, 0);
  }
  int
  mulib_xlib_window_title (mulib_xlib_t *c, mulib_xlib_window_t w,
                           const char *title)
  {
    if (!c || !w || !title)
      return 0;
    (void)c->title (c->display, w, title);
    return 1;
  }
  int
  mulib_xlib_window_map (mulib_xlib_t *c, mulib_xlib_window_t w)
  {
    if (!c || !w)
      return 0;
    (void)c->map (c->display, w);
    return 1;
  }
  int
  mulib_xlib_window_destroy (mulib_xlib_t *c, mulib_xlib_window_t w)
  {
    if (!c || !w)
      return 0;
    (void)c->destroy (c->display, w);
    return 1;
  }
  int
  mulib_xlib_flush (mulib_xlib_t *c)
  {
    if (!c)
      return 0;
    (void)c->flush (c->display);
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
