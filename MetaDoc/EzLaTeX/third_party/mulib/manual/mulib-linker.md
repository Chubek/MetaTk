# mulib-linker

A **single flat image** linker: owned named symbols and checked relocation
fields, composed with `mulib-emitter`. It is not an ELF/COFF/Mach-O reader,
section placer, dynamic loader, PLT/GOT builder, or archive linker.

## Integration and ownership

Define `MULIB_IMPL_LINKER` and `MULIB_IMPL_EMITTER` explicitly in one C11 TU.
No dependency switch is enabled automatically. Declarations support C++17;
there are no platform-specific requirements.

`mulib_linker_create(max_symbols, max_relocations)` allocates fixed-limit
tables and returns an owned opaque object. Zero limits are allowed. Limit
multiplication and allocations are checked; invalid/overflowing limits and
allocation failure return NULL. Symbol names are separately allocated as
needed. `mulib_linker_destroy` frees all tables/names; NULL is harmless.
The linker never owns the emitter or its storage. Objects are not thread-safe.

## Symbols and relocations

`mulib_linker_define(linker, name, value, absolute)` defines either an
image-relative byte offset (`absolute=0`) or an absolute address (`1`). Names
are copied, case-sensitive, nonempty printable ASCII without spaces, and at
most 128 bytes. Forward references are interned by `relocate`; a later `define`
resolves them. A second definition is an error, even with the same value.

`mulib_linker_relocate(linker, name, field_offset, kind, addend)` records:

| Kind | Field | Formula and accepted result |
| --- | --- | --- |
| `MULIB_LINKER_ABS32` | 4 bytes | `S + A`, unsigned 32-bit |
| `MULIB_LINKER_ABS64` | 8 bytes | `S + A`, unsigned 64-bit |
| `MULIB_LINKER_PC32` | 4 bytes | `S + A - P`, signed 32-bit |

`S` is the resolved symbol address, `A` the signed explicit addend, and `P`
the address of the **first field byte**, not its end. AMD64 rel32 branches use
addend `-4` to make the displacement relative to the following instruction.
Signed PC-relative arithmetic handles cancellation and negative results even
at base zero; it does not rely on overflowing signed integers. Relocation
fields cannot overlap, and field offsets cannot wrap. Ordinary bytes may be
emitted after recording a relocation. Failed recording/definition is atomic,
including the interned-name count.

`mulib_linker_apply(linker, image, base_address, &bad_index)` first validates
**all** relocations, then patches all in the emitter's byte order. Failure
leaves image bytes and size untouched. The image range itself must not wrap
64-bit addressing. Relative symbols must be within the image or exactly at
its end. Absolute symbols may designate caller-resolved external addresses.
All referenced symbols must be defined. Reapplying at another base is allowed;
the original addends live in relocation records, not patched bytes.

`symbol_count` includes undefined interned names; `relocation_count` counts
fields. Neither includes capacity. Lookups and overlap checks are linear,
so creating many records is quadratic overall; callers choose appropriate
fixed limits. No filesystem, callbacks, or hidden external resolver exists.

## Failures

Zero means success. Results distinguish `EINVAL`, `ENOMEM`, `ELIMIT`,
`EDUPLICATE`, `EUNDEFINED`, `ERANGE`, and `EOVERLAP`. On apply,
`bad_relocation` identifies the first invalid relocation, or is `SIZE_MAX` for
success and non-relocation errors. The optional pointer may be NULL. Range
errors include overflow, unrepresentable displacement, patch outside the
image, relative definition outside the image, and image-base wraparound.

## Runnable example

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  examples/modules/linker.c -o /tmp/opencode/mulib-linker
/tmp/opencode/mulib-linker
```

The example resolves a forward AMD64 jump. `tests/test_codegen.c` covers
undefined/duplicate symbols, atomic failure, both byte orders, backward
branches at base zero, signed boundaries/cancellation, limits and overflow.
