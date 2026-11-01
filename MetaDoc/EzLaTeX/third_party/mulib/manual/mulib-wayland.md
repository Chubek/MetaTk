# mulib-wayland

Native-endian Wayland frame utilities and an optional runtime-loaded client
connection. Not a generated protocol binding, renderer or compositor.

## Integration and framing

Define `MULIB_IMPL_DYNLOAD`, `MULIB_IMPL_WAYLAND` in one C11 translation unit;
link `-ldl` where required. No build-time Wayland headers/libraries needed.
`frame_parse(bytes,length,&frame,&consumed)` reads the 8-byte native-endian
header, validates nonzero object id, size >=8 and multiple of four, then borrows
the payload. Unaligned input is safe. It consumes one frame, not an entire pipeline.
MORE=0 means incomplete; OK=1, INVALID=-1. Output unchanged and consumed zero
on MORE/error. Payload view remains valid only while source bytes do.

`frame_encode(&frame,&bytes,&length)` allocates bytes released with `free`,
or returns INVALID/NOMEM=-2 without touching outputs. Payload must be pre-encoded,
4-byte aligned in length (pointer need not be), <=65,524 bytes. A frame's maximum
size is 65,532 bytes. This checks only framing, **not** opcode/interface legality,
argument encoding, string termination, object lifetime or SCM_RIGHTS fd counts.
The caller must coordinate ancillary descriptors and protocol semantics. Never
write raw encoded frames into a connection managed by libwayland's proxy state.

## Optional client runtime

`connect(name)` loads `libwayland-client.so.0` (`.dylib` on macOS) and opens a
display; NULL name uses native environment defaults. Returns opaque owned pointer,
NULL on missing runtime/allocation/connection failure. No library loaded during
frame operations. `disconnect` closes/free connection and loader; NULL safe.
Destroy native proxies before disconnect. `display` and `fd` are borrowed; do
not close the fd. `dispatch_pending`, blocking `roundtrip` and nonblocking `flush`
return native >=0 or -1; flush may set EAGAIN. Use native Wayland APIs through
the borrowed display for registry/proxy creation and polling/prepare-read loops.
There is no implicit blocking event dispatch or thread-safety layer.

`cc -std=c11 -Iinclude examples/modules/wayland.c -ldl -o wayland` runs without
a compositor/runtime. Tests cover framing, truncation and unaligned input; real
display integration is optional and not asserted by hardware-free tests.
`tests/test_desktop_adapters.c` uses a synthetic loader to verify call forwarding,
connection cleanup and every symbol-resolution failure without external runtimes.
