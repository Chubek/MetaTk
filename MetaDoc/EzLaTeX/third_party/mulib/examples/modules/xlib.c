#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_XLIB
#include "mulib-xlib.h"
#include <assert.h>
int main(int argc, char **argv) {
    (void)argv;
    /* With no arguments this example needs neither libX11 nor an X server. */
    if (argc > 1) {
        mulib_xlib_t *client = mulib_xlib_open(NULL);
        if (!client) return 1;
        mulib_xlib_window_t window = mulib_xlib_window_create(client,0,0,320,200);
        assert(window && mulib_xlib_window_title(client,window,"MuLIB"));
        assert(mulib_xlib_window_map(client,window) && mulib_xlib_flush(client));
        assert(mulib_xlib_window_destroy(client,window)); mulib_xlib_close(client);
    }
    return 0;
}
