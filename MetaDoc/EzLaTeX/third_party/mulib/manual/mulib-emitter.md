# mulib-emitter

An allocation-free, bounded byte image builder. Define `MULIB_IMPL_EMITTER` in
one C11 translation unit; no dependencies or platform feature macros.
Declarations support C++17. Eight-bit bytes and the standard fixed-width
integer types are required.

## Storage and API

Initialize `mulib_emitter_t` using `mulib_emitter_init(&e, storage, capacity,
MULIB_EMITTER_LITTLE)` or `MULIB_EMITTER_BIG`. The emitter **borrows** storage:
it never reallocates, frees, or takes ownership. Storage must remain live and
writable. NULL storage is legal only with zero capacity. `MULIB_EMITTER_INIT`
describes an empty zero-capacity emitter. Reinitializing resets size but does
not erase storage. Do not edit the structure's fields manually.

- `bytes(e, bytes, length)` appends; overlapping buffers use `memmove`.
- `reserve(e, length, &offset)` appends zeroes and returns their start. The
  offset output may be NULL, and is unchanged on failure.
- `uint(e, value, width)` appends a 1..8-byte unsigned integer in image order.
- `patch(e, offset, bytes, length)` replaces already-emitted bytes, including
  overlapping source bytes; it never extends size.
- `patch_uint(e, offset, value, width)` is the endian integer patch equivalent.

The structure exposes `data`, `size`, `capacity`, and `order` for observation
and composition with the assembler/linker. It owns no resources and needs no
destructor. Copies share the same underlying bytes and are not independent
images. Concurrent mutation requires caller synchronization.

## Bounds and failures

Zero-length byte/reserve/patch operations are valid, including NULL byte
pointers; a zero-length patch still requires `offset <= size`. Integer width
zero is invalid. Integers that do not fit the chosen width are rejected, not
silently truncated. To store a signed two's-complement field, first validate
its signed range, then convert to the corresponding unsigned field width.

Every function returns zero on success. `MULIB_EMITTER_EINVAL` means invalid
arguments/state/order/width, `ERANGE` means an integer cannot fit or a patch is
outside the emitted image, and `ENOSPACE` means append capacity was exceeded.
Every failure preserves all bytes, size, and auxiliary outputs. Checks use
subtraction before addition, so `SIZE_MAX` lengths/offsets do not wrap. No
allocation or hidden sticky-error state exists.

## Runnable example

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  examples/modules/emitter.c -o /tmp/opencode/mulib-emitter
/tmp/opencode/mulib-emitter
```

The example writes a big-endian header, reserves a patch field, and rejects
integer truncation. `tests/test_codegen.c` also tests both byte orders,
overlapping buffers, empty images, full capacity, and overflowing ranges.
