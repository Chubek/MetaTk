# mulib-dbus

An actual **D-Bus version 1 binary wire codec**. It emits/parses the 16-byte
fixed header, aligned `a(yv)` header fields, body signature and binary values.
It does not encode JSON or imitate D-Bus with a different protocol.
No bus daemon, sockets, authentication, message routing, introspection, UNIX
file-descriptor passing or OS integration is provided.

## Integration

Define `MULIB_IMPL_DBUS`, `MULIB_IMPL_ENDIAN`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_STRBUF`, and `MULIB_IMPL_UTF8` in one C11 translation unit before
inclusion. Endian helpers implement unaligned 32-bit loads/stores; binary
buffers assemble frames; strbuf builds signatures/copies text; UTF8 validates
wire strings. No external libraries. Binary64 `double` support requires an
8-byte IEEE binary64 C double (otherwise type `d` is rejected).

## Supported values

`mulib_dbus_value_t` is a tagged tree. Zero initialize it before building:

| type | storage |
| --- | --- |
| `y`, `b`, `q`, `u`, `t` | `u64` (8-bit byte, boolean 0/1, uint16/32/64) |
| `n`, `i`, `x` | `i64` (signed 16/32/64, range checked) |
| `d` | `real` (binary64, including nonfinite bit patterns) |
| `s`, `o`, `g` | `text` and explicit `length` (string, object path, signature) |
| `a` | `children/count`, plus one element `signature`, e.g. `{sv}` or `(iu)` |
| `(` | nonempty struct `children/count` |
| `{` | two dict-entry children, basic key then value; only array elements |
| `v` | one child, plus its single complete `signature` |

Arrays may be empty. Structs may not. Arrays of structs, dictionaries,
`a{sv}`, nested arrays and nested variants are supported. UNIX_FD (`h`) is
**unsupported**, including inside unknown headers. Nonzero UNIX_FDS header
counts are rejected; a zero count is accepted and discarded. String and path
values reject embedded NULs (unlike JSON); signatures are <=255 bytes. No
percent-decoding or coercion occurs.

`mulib_dbus_signature_valid(bytes,length)` validates a complete signature;
empty is valid and multiple top-level types are allowed. `mulib_dbus_signature`
derives malloc-owned text from values. Arrays need an explicit element signature
even when nonempty; variants retain theirs. This utility derives type layout,
not scalar ranges or all dynamic variant contents; encoding validates values.

## Messages and validation

`mulib_dbus_message_t` has endian `'l'`/`'B'`, type 1 call / 2 return / 3 error /
4 signal, flags (only low three bits), and nonzero serial. Optional header
strings use the appropriate D-Bus path/member/interface/error/bus-name grammar.
Calls require path/member, returns require nonzero reply_serial, errors also
require error_name, signals require path/interface/member. Message `signature`
may be NULL on encoding; if supplied, it must equal the derived body signature.

`mulib_dbus_encode(&message,limits,&bytes,&length)` borrows the entire value tree
and returns malloc-owned binary bytes. On success free bytes with `free`.
Never apply decoded-object free functions to a stack-built encoder tree.

`mulib_dbus_decode(bytes,length,limits,&message,&consumed)` returns one owned
message. `mulib_dbus_message_free` releases it recursively; `value_free` releases
a decoded value's contents but not the value allocation itself. Do not overwrite
a live parsed output. Decode does not borrow any input after returning.

Results: OK=1, MORE=0, INVALID=-1, LIMIT=-2, NOMEM=-3. On non-OK outputs remain
unchanged and consumed is zero. MORE means the declared frame is incomplete;
retain the entire input and append/retry. A complete frame whose contents are
truncated relative to its internal lengths is INVALID, not MORE. Successful
decode consumes precisely one frame, leaving trailing/pipelined bytes untouched;
compare consumed to input length if trailing bytes must be forbidden.

Both byte orders, alignment (including zero padding), boolean representation,
container boundaries, terminators, UTF-8, signature grammar and required headers
are checked. The reserved Local path/interface, header code zero, duplicate fields, wrong known-field variant
types, zero serial/reply_serial, unsupported types, reserved flags, and body
signature/length disagreement are rejected. Unknown nonduplicate header fields
are decoded/validated then ignored when their variant uses supported types.
Reencoding may change field order and omit unknown fields; it preserves message
semantics for the documented fields, not byte-for-byte identity.

Zero limits select 16 MiB total frame bytes, 65536 value-tree nodes (headers
included), and combined container depth 32. These bounds apply before allocations
and prevent wire-to-tree amplification. Depth is a deliberately smaller subset
than some D-Bus implementations; increasing max_depth past 32 has no effect.
The protocol's 128 MiB message and 64 MiB array/header-array ceilings apply
even if a larger configurable byte limit is supplied.
Strings, arrays and header values count toward frame/node limits. No network
authentication claim is implied by accepting a valid binary frame.

## Runnable example

[`examples/modules/dbus.c`](../examples/modules/dbus.c) encodes a method call,
checks its binary header, then decodes the string body:

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/dbus.c -o ./dbus
./dbus
```
