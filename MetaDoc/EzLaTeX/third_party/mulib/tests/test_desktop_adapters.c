/* Exercise runtime adapters without a display server or shared-library install.
 * Provide a deterministic dynload implementation in this isolated executable. */
#define MULIB_IMPL_WAYLAND
#define MULIB_IMPL_XLIB
#include "mulib-wayland.h"
#include "mulib-xlib.h"
#include <assert.h>
#include <string.h>

static int wayland_marker, xlib_marker, display_marker;
static int fail_load, fail_symbol, fail_connect, lookups, loads, unloads, disconnects, requests;
static struct wl_display *fake_wl_connect(const char *name) {
    assert(name && !strcmp(name,"test"));
    return fail_connect ? NULL : (struct wl_display *)&display_marker;
}
static void fake_wl_disconnect(struct wl_display *display) {
    assert(display == (struct wl_display *)&display_marker); ++disconnects;
}
static int fake_wl_fd(struct wl_display *display) { assert(display); return 17; }
static int fake_wl_pending(struct wl_display *display) { assert(display); return 2; }
static int fake_wl_roundtrip(struct wl_display *display) { assert(display); return 3; }
static int fake_wl_flush(struct wl_display *display) { assert(display); return 4; }
static struct _XDisplay *fake_x_open(const char *name) {
    assert(name && !strcmp(name,"test"));
    return fail_connect ? NULL : (struct _XDisplay *)&display_marker;
}
static int fake_x_close(struct _XDisplay *display) { assert(display); ++disconnects; return 0; }
static int fake_x_screen(struct _XDisplay *display) { assert(display); return 1; }
static int fake_x_width(struct _XDisplay *display, int screen) { assert(display && screen == 1); return 1920; }
static int fake_x_height(struct _XDisplay *display, int screen) { assert(display && screen == 1); return 1080; }
static unsigned long fake_x_root(struct _XDisplay *display, int screen) { assert(display && screen == 1); return 100; }
static unsigned long fake_x_create(struct _XDisplay *display, unsigned long parent, int x, int y,
    unsigned w, unsigned h, unsigned border, unsigned long border_pixel, unsigned long background) {
    assert(display && parent == 100 && x == 10 && y == 20 && w == 320 && h == 200);
    assert(!border && !border_pixel && !background); ++requests; return 101;
}
static int fake_x_title(struct _XDisplay *display, unsigned long window, const char *title) {
    assert(display && window == 101 && !strcmp(title,"MuLIB")); ++requests; return 0;
}
static int fake_x_window(struct _XDisplay *display, unsigned long window) {
    assert(display && window == 101); ++requests; return 0;
}
static int fake_x_flush(struct _XDisplay *display) { assert(display); ++requests; return 0; }

int mulib_dynload_open(mulib_dynload_t *library, const char *path, char *error, size_t cap) {
    (void)error; (void)cap; assert(library && !library->handle && path);
    ++loads; if (fail_load) return MULIB_DYNLOAD_EIO;
    library->handle = strstr(path,"wayland") ? (void *)&wayland_marker : (void *)&xlib_marker;
    library->owned = 1; return 0;
}
int mulib_dynload_close(mulib_dynload_t *library, char *error, size_t cap) {
    (void)error; (void)cap;
    if (library->handle) ++unloads;
    library->handle = NULL; library->owned = 0; return 0;
}
#define BIND(symbol_name, function) \
    if (!strcmp(name, symbol_name)) { \
        void (*address)(void) = (void (*)(void))function; \
        assert(sizeof(address) == sizeof(*symbol)); \
        memcpy(symbol,&address,sizeof(*symbol)); return 0; \
    }
int mulib_dynload_symbol(const mulib_dynload_t *library, const char *name, void **symbol, char *error, size_t cap) {
    (void)error; (void)cap; assert(library && library->handle);
    ++lookups; if (fail_symbol == lookups) return MULIB_DYNLOAD_EIO;
    if (library->handle == &wayland_marker) {
        BIND("wl_display_connect",fake_wl_connect)
        BIND("wl_display_disconnect",fake_wl_disconnect)
        BIND("wl_display_get_fd",fake_wl_fd)
        BIND("wl_display_dispatch_pending",fake_wl_pending)
        BIND("wl_display_roundtrip",fake_wl_roundtrip)
        BIND("wl_display_flush",fake_wl_flush)
    } else {
        BIND("XOpenDisplay",fake_x_open)
        BIND("XCloseDisplay",fake_x_close)
        BIND("XDefaultScreen",fake_x_screen)
        BIND("XDisplayWidth",fake_x_width)
        BIND("XDisplayHeight",fake_x_height)
        BIND("XRootWindow",fake_x_root)
        BIND("XCreateSimpleWindow",fake_x_create)
        BIND("XStoreName",fake_x_title)
        BIND("XMapWindow",fake_x_window)
        BIND("XDestroyWindow",fake_x_window)
        BIND("XFlush",fake_x_flush)
    }
    assert(!"unexpected symbol"); return MULIB_DYNLOAD_EIO;
}
#undef BIND
static void reset(void) {
    fail_load = fail_symbol = fail_connect = lookups = loads = unloads = disconnects = requests = 0;
}
static void wayland(void) {
    mulib_wayland_client_t *client;
    reset(); client = mulib_wayland_connect("test"); assert(client && loads == 1 && lookups == 6 && !unloads);
    assert(mulib_wayland_display(client) == &display_marker && mulib_wayland_fd(client) == 17);
    assert(mulib_wayland_dispatch_pending(client) == 2 && mulib_wayland_roundtrip(client) == 3 && mulib_wayland_flush(client) == 4);
    mulib_wayland_disconnect(client); assert(unloads == 1 && disconnects == 1);
    for (int i = 1; i <= 6; ++i) {
        reset(); fail_symbol = i; assert(!mulib_wayland_connect("test") && lookups == i && unloads == 1 && !disconnects);
    }
    reset(); fail_connect = 1; assert(!mulib_wayland_connect("test") && unloads == 1 && !disconnects);
    reset(); fail_load = 1; assert(!mulib_wayland_connect("test") && !unloads && !lookups);
}
static void xlib(void) {
    mulib_xlib_t *client; mulib_xlib_window_t window; unsigned width, height;
    reset(); client = mulib_xlib_open("test"); assert(client && loads == 1 && lookups == 11 && !unloads);
    assert(mulib_xlib_display(client) == &display_marker && mulib_xlib_size(client,&width,&height) && width == 1920 && height == 1080);
    assert(!mulib_xlib_window_create(client,32768,0,1,1) && !mulib_xlib_window_create(client,0,0,65536,1) && !requests);
    window = mulib_xlib_window_create(client,10,20,320,200); assert(window == 101);
    assert(mulib_xlib_window_title(client,window,"MuLIB") && mulib_xlib_window_map(client,window));
    assert(mulib_xlib_flush(client) && mulib_xlib_window_destroy(client,window) && requests == 5);
    mulib_xlib_close(client); assert(unloads == 1 && disconnects == 1);
    for (int i = 1; i <= 11; ++i) {
        reset(); fail_symbol = i; assert(!mulib_xlib_open("test") && lookups == i && unloads == 1 && !disconnects);
    }
    reset(); fail_connect = 1; assert(!mulib_xlib_open("test") && unloads == 1 && !disconnects);
    reset(); fail_load = 1; assert(!mulib_xlib_open("test") && !unloads && !lookups);
}
int main(void) { wayland(); xlib(); return 0; }
