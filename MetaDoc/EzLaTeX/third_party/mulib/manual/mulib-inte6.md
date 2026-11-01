# mulib-inte6

Backward-compatible historical spelling of [`mulib-inet6`](mulib-inet6.md).
The implementation now lives in `mulib-inet6.h`, while old source including
`mulib-inte6.h` and defining `MULIB_IMPL_INET6` continues to work unchanged.

## Old and new integration

For existing canonical symbols, retain the original setup:

```c
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#include "mulib-inte6.h"
```

The forwarding include also supports including declarations first, defining
`MULIB_IMPL_INET6` later, then including `mulib-inte6.h` again in the same
translation unit. Canonical and historical headers may be included together;
their implementation guards prevent duplicate definitions.

For the additional typo-spelled forwarding functions, also explicitly define
`MULIB_IMPL_INTE6` once. It is a unique implementation switch: it emits only
`mulib_inte6_*` wrappers and never defines `MULIB_IMPL_INET6` or any dependency.
The canonical implementation and dependencies must be linked separately or
enabled explicitly in the same translation unit. C++ callers receive C linkage.

## Public facade

`mulib_inte6_t` is exactly the `mulib_inet6_t` value type: sixteen owned bytes in
network order. Four actual public functions forward without extra state:

* `mulib_inte6_parse(text,length,&address)` -> `mulib_inet6_parse`.
* `mulib_inte6_format(&address,out,capacity)` -> `mulib_inet6_format`.
* `mulib_inte6_network(&address,prefix,&out)` -> `mulib_inet6_network`.
* `mulib_inte6_contains(&network,prefix,&address)` -> `mulib_inet6_contains`.

They can be used as function pointers and mixed with canonical functions. There
is no separate algorithm, allocator, cache, destructor or native network library.
No third-party package is required.

## Supported syntax and limits

Literal input is 2..45 explicit bytes: eight groups of one to four hex digits,
or one `::` replacing at least one zero group. A final strict dotted IPv4 suffix
occupies two groups. Brackets, scopes, ports, whitespace, embedded NUL,
multiple compressions and incorrect group counts are rejected. The parser uses
MuLIB combinators; the IPv4 dependency may allocate temporary captures, released
before return. The facade retains no input or output pointers.

Formatting uses lowercase hexadecimal and the longest zero run of length >=2,
with the first run winning ties. Mapped IPv4 is formatted as hexadecimal too.
Forty output bytes suffice including NUL. Prefix operations accept 0..128;
network output may alias its input. Other source/output storage is disjoint.
Parse/format/network return 1 on success and 0 on invalid input, insufficient
capacity or underlying parser failure; output is unchanged on failure.
Membership returns 0 for invalid arguments as well as a nonmember.

## Example and tests

`cc -std=c11 -Iinclude examples/modules/inte6.c -o inte6` builds a runnable
example exercising both spellings. `tests/test_dns_stack.c` tests the actual
forwarding functions; existing network/protocol suites continue using the
historical header with `MULIB_IMPL_INET6`. New applications should prefer the
canonical filename and API documented in `manual/mulib-inet6.md`.
