# mulib-getaddr

Owned numeric address values, strict portable literal parsing, and optional
synchronous operating-system host lookup. It is independent of the DNS stack:
native name service may use hosts files, DNS or other platform services.

## Integration and platform guards

Define `MULIB_IMPL_GETADDR`, `MULIB_IMPL_INET6`, `MULIB_IMPL_INET4` and
`MULIB_IMPL_PARSEC` explicitly once and include `mulib-getaddr.h`. The headers do
not enable dependency switches. Declarations support C++ via C linkage. The
literal path has no native networking-library dependency.

`MULIB_GETADDR_HAS_SYSTEM` is 1 when native `getaddrinfo` is available to the
implementation: Windows, Apple, or Unix/Linux with a visible POSIX 2001-or-newer
feature selection (`_POSIX_C_SOURCE >= 200112L`, `_XOPEN_SOURCE >= 600`,
`_GNU_SOURCE`, or `_DEFAULT_SOURCE`). Otherwise it is 0. Select feature-test
macros **before all system headers**, preferably as compiler options. Do not
force `HAS_SYSTEM=1` on an unsupported platform. Defining
`MULIB_GETADDR_NO_SYSTEM` disables autodetection and keeps literal-only builds
portable. Keep configuration consistent between translation units.

On POSIX, for example:

```
cc -std=c11 -D_POSIX_C_SOURCE=200809L -Iinclude examples/modules/getaddr.c -o getaddr
```

Windows applications must initialize Winsock themselves and link `ws2_32`.
There is no implicit `WSAStartup`/`WSACleanup`. Windows code is guarded, but the
local verification platform is Linux; no Windows runtime behavior is claimed.
An ordinary strict C11 build without feature macros still supports all literals;
requesting system resolution returns `UNSUPPORTED` if not compiled in.

## Addresses and lookup

`mulib_getaddr_address_t` owns `family` (4 or 6), `bytes[16]` in network order,
`port` in host order, and `scope_id` (zero for IPv4/literals). IPv4 uses its
first four bytes; remaining bytes are zero. Values can be copied freely. No
sockaddr pointers or native result list escape the function.

`resolve(host,port,family,flags,addresses,capacity,&count)` takes a NUL-terminated
host of 1..253 bytes. Family is 0 (either), 4 or 6. Port is a numeric `uint16_t`;
there are no service strings, passive/null hosts or socket options. Flags are
zero or `MULIB_GETADDR_SYSTEM`. Inputs and output storage must not overlap.

Strict dotted IPv4 and hexadecimal IPv6 literals are always tried locally,
including final embedded IPv4 in IPv6. Leading-zero IPv4 octets, shortened
numeric decimal addresses, IPv6 brackets, scopes/zones, whitespace and non-ASCII
are rejected. A family-mismatched valid literal returns `NOTFOUND`. Without
SYSTEM, an ordinary hostname returns `NOTFOUND` without performing I/O.

With SYSTEM, hostnames use native `getaddrinfo` with a stream-socket hint and
requested family, then distinct addresses are copied in OS order. Host text
is limited to ASCII letters/digits/hyphens/underscores/dots, with nonempty labels
<=63 bytes and an optional final dot. No IDNA conversion, search-policy control,
canonical hostname output or DNS TTL is available. Native name-service policy
is operating-system dependent. A preliminary numeric-only native check rejects
legacy numeric forms (such as hexadecimal IPv4) rather than reinterpreting them
as strict literals or hostnames. Returned IPv6 scope IDs are preserved even though
scoped literal input is intentionally unsupported.

## Capacity, errors and ownership

`OK` is 1. `INVALID` covers arguments/syntax; `NOTFOUND` covers no address;
`UNSUPPORTED` covers a system request in a literal-only build; literal-parser
failures (including temporary capture allocation failure) have no separate
diagnostic and may return `INVALID`. Native failures
other than `EAI_NONAME` map to `SYSTEM_ERROR`. The output array is unchanged on
failure. For insufficient capacity, `LIMIT` publishes the required distinct
count; NULL output with capacity zero is a sizing query. Native lists longer
than 1024 nodes also return `LIMIT`, but leave count unchanged because the full
result was not enumerated. Other failures leave count unchanged.

All native result lists are freed before return, including errors. Native
`getaddrinfo` may allocate and block; allocation failure maps to `SYSTEM_ERROR`.
This module has no injected allocator, async mode, timeout/cancellation or cache.
Only caller-owned copied values survive. The limited native duplicate scan is
O(nodes²), with at most 1024 nodes.

`format(&address,out,capacity)` emits only the numeric bytes, without scope, port
or brackets, using the IPv4/IPv6 formatter. It returns `LIMIT` if the buffer is
too small and leaves it unchanged; invalid family returns `INVALID`. Forty bytes
always suffice. It does not convert the value into a native socket address.

## Example and tests

`examples/modules/getaddr.c` checks literal parsing, formatting, sizing, and
guarded localhost lookup. `tests/test_dns_stack.c` is exercised both with plain
strict C11 and with POSIX feature macros, covering both platform branches.
