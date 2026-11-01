# mulib-inet6

Portable IPv6 literals, canonical hexadecimal formatting, and prefix operations.
`mulib-inet6.h` is the canonical header; the historical `mulib-inte6.h` still
includes it and preserves existing `MULIB_IMPL_INET6` applications.

## Integration and ownership

Define `MULIB_IMPL_PARSEC`, `MULIB_IMPL_INET4`, and `MULIB_IMPL_INET6` exactly once
before including `mulib-inet6.h` in an implementation translation unit. The
header does not enable its dependency macros. Other translation units include
declarations without the switches. Public declarations have C linkage for C++.
No socket API, system networking library or third-party package is needed.

`mulib_inet6_t` owns sixteen freely copyable network-order address bytes. Parse,
format and prefix functions do not retain pointers or allocate output. Parsing
uses the existing MuLIB parser combinators; a final dotted IPv4 portion reuses
`mulib-inet4`, whose parser may temporarily allocate capture storage. All such
storage is released before return. There is no address destructor.

## Literal grammar

`mulib_inet6_parse(text,length,&address)` accepts exactly eight hexadecimal
groups of one through four digits, or fewer groups with exactly one `::`
replacing at least one all-zero group. Both uppercase and lowercase hex are
accepted. A final strict dotted-decimal IPv4 address consumes two groups.
Input length must be 2..45 bytes; an explicit length excludes any trailing NUL.

Examples: `::`, `::1`, `2001:DB8::1`, `1:2:3:4:5:6:7:8`,
and `::ffff:192.0.2.1`. A dotted suffix must be at the end and contain four
decimal octets without leading zeroes. Multiple `::`, excess/missing groups,
five-digit groups, trailing single colons and embedded NUL fail. There is no
bracket, port, URI escape, zone ID, IPvFuture, whitespace or hostname syntax.
Unsupported forms return zero instead of being partly consumed.

## Formatting and prefixes

`mulib_inet6_format(&address,out,capacity)` emits lowercase hex without group
leading zeroes and compresses the longest zero run of length at least two.
The first run wins ties. Mapped/embedded IPv4 addresses also format as hex,
not dotted decimal. Forty bytes suffice including the terminator. Formatting
does not include brackets, a scope or port and is independent of native APIs.

`mulib_inet6_network(&address,prefix,&out)` clears host bits for prefix 0..128;
output may alias the input. Prefix zero yields all zeroes; prefix 128 copies
the address. `mulib_inet6_contains(&network,prefix,&address)` tests membership
while ignoring network host bits. There is no special mapped-address policy.

Parse, format and network return 1 on success, 0 on invalid arguments/syntax,
insufficient output capacity or underlying parser failure. Their outputs remain
unchanged on failure. `contains` returns 1 for membership and 0 for nonmembership
or invalid arguments. Formatting input and text output must be disjoint. There
is no fine-grained error object, configuration state or global mutable state.

## Compatibility and verification

The implementation moved from `mulib-inte6.h` without changing the existing
`mulib_inet6_*` API or `MULIB_IMPL_INET6` switch. Use `MULIB_IMPL_INTE6` only if
you also want the new `mulib_inte6_*` forwarding functions; canonical users do
not need that switch. See [`mulib-inte6`](mulib-inte6.md).

`cc -std=c11 -Iinclude examples/modules/inet6.c -o inet6` builds an assert-based
canonicalization/prefix example. Existing `tests/test_network.c` covers malformed
forms, every prefix and deterministic native `inet_pton` roundtrips on POSIX;
`tests/test_dns_stack.c` adds canonical/compatibility and getaddr composition.
