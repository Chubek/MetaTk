# mulib-xlib

Optional runtime-loaded Xlib display/default-screen and simple-window helpers.
No build-time X11 dependency, event loop, drawing API, input injection or WM policy.

## Integration and runtime

Define `MULIB_IMPL_DYNLOAD`, `MULIB_IMPL_XLIB` before including the header in one
C11 translation unit; link `-ldl` where required. `open(name)` loads `libX11.so.6`
(`libX11.dylib` on macOS) and opens a display. NULL name uses Xlib's DISPLAY
default. NULL return means loader/connection/allocation failure; no global error
buffer. Loading a missing runtime is a normal failure, not a link-time dependency.

The opaque client owns display and library. `close` releases both, including all
server resources belonging to the connection, and is NULL safe. `display` borrows
the native Display pointer until close; native Xlib interoperability requires the
application's own headers/loader or linked symbols. This API is single-threaded;
it does not call XInitThreads or install process-global error handlers.

## Windows and asynchronous errors

`size` retrieves default-screen dimensions and returns 1 or 0 for invalid input.
`window_create(client,x,y,width,height)` queues a simple zero-border, pixel-zero
background window on the default root. Coordinates are -32768..32767, dimensions
1..65535. Returns native XID or zero for local invalid input. Windows belong to
their originating display; never pass foreign or destroyed XIDs.

`window_title`, `window_map`, `window_destroy` return 1 when a request was queued,
0 for local invalid arguments. `flush` submits requests. These are **not** server
acknowledgements: protocol errors and connection loss use Xlib's existing error
and fatal-I/O policies, which may terminate the process. Handle these with native
Xlib facilities when needed. Title uses XStoreName legacy encoding, not an EWMH
UTF-8 property. Applications supply native event handling and WM_DELETE_WINDOW
support themselves. Queued requests copy borrowed titles; windows need no heap free.

`cc -std=c11 -Iinclude examples/modules/xlib.c -ldl -o xlib`; no arguments is
hardware-free, an extra argument explicitly opens the default display and queues
a brief window creation/map/destroy sequence. Core tests validate NULL operations;
they do not claim successful communication with a live X server. The isolated
`tests/test_desktop_adapters.c` additionally verifies loader failure cleanup and
native call forwarding using a deterministic synthetic loader.
