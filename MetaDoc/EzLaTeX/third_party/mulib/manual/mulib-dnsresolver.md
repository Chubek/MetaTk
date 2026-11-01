# mulib-dnsresolver

Resolve one positive A or AAAA RRset through an application transport callback,
optionally consulting and updating `mulib-dnscache`. This layer is deliberately
not an iterative Internet resolver.

## Integration

Define these switches explicitly, exactly once across the program:

```c
#define MULIB_IMPL_DNS
#define MULIB_IMPL_DNSCLIENT
#define MULIB_IMPL_DNSCACHE
#define MULIB_IMPL_DNSRESOLVER
#include "mulib-dnsresolver.h"
```

The cache implementation is a link dependency even when a particular resolver
uses a NULL cache. There are no native networking libraries or third-party
requirements. C++ callers receive C-linkage declarations. No dependency switch
is implicitly defined by any header.

## Initialization and request storage

`init(&resolver,transport,user,cache,first_id)` borrows the callback state and
optional initialized cache. It does not allocate or free either. The transport
contract is described in [`mulib-dnsclient`](mulib-dnsclient.md). Serialize calls
that share a resolver or cache; there are no internal locks.

`resolve(&resolver,name,length,type,now,query,query_capacity,reply,reply_capacity,
&result)` accepts only `MULIB_DNS_A` or `MULIB_DNS_AAAA`. The name is normalized
before lookup. `now` uses the cache's caller-selected clock in seconds. Result is
an owned `mulib_dnscache_result_t` value: type, remaining/effective TTL, 1..8
addresses in network order. IPv4 uses the first four bytes of a zeroed 16-byte
slot. No address or name pointer is retained from either packet.

The query and reply workspaces are caller-owned, disjoint byte arrays. They
must also not overlap inputs, outputs, the resolver, cache, records, or callback
state that will be read. A typical small-zone use supplies 512-byte arrays;
larger replies may use up to 65535 bytes. A cache hit leaves them untouched and
permits both pointers/capacities to be NULL/zero. A cache miss needs both buffers.
The caller may overwrite the workspaces as soon as the function returns.

## Resolution and validation policy

On a miss the resolver makes one IN query with RD, using `next_id`, then
increments that 16-bit field modulo 65536. It validates the full response and
query correspondence through `mulib-dnsclient`. IDs are sequential, not random;
the caller must choose/reset IDs and implement authenticated/peer-checked
transport as appropriate. This module is not a security boundary or DNSSEC
validator.

Only answer-section IN records with exactly the requested owner and type are
collected. Authority and additional records are never used as addresses or
cached. Unrelated answers are ignored after structural validation. A matching
CNAME returns `UNSUPPORTED`; no alias is followed. A successful but empty answer
returns `NOTFOUND`, as does RCODE NXDOMAIN. Other RCODE failures and TC propagate
as `RESPONSE` and `TRUNCATED`. More than eight matching records returns `LIMIT`
without publishing a partial result. Duplicate wire addresses count as records;
no deduplication changes RRset limits or TTL calculation.

The result TTL is the minimum matching wire TTL; any TTL with its high bit set
is treated as zero. A zero-TTL result is returned but not retained in cache.
No negative answer is cached. A disabled/full zero-capacity cache does not make
an otherwise valid resolution fail. Output is unchanged on failure, though the
callback/workspaces, transaction counter and lazy expiry may have changed.

## Limits and unsupported features

All codec restrictions apply, including ASCII-only restricted labels, <=1024
total records, <=128 pointer hops and A/AAAA/NS/CNAME/PTR/MX/TXT RDATA only.
SOA-bearing negative responses, OPT/EDNS and DNSSEC packets are rejected.
There are no socket operations, search domains, IDNA, aliases, retries,
iterative delegation, TCP fallback, negative cache, cancellation or async API.
For operating-system name-service behavior, use `mulib-getaddr` instead.

## Example and tests

`examples/modules/dnsresolver.c` wires the resolver to a local static
`mulib-nameserver` callback and verifies that its second call is a TTL-cache hit.
That example also explicitly defines `MULIB_IMPL_NAMESERVER`.
Build with `cc -std=c11 -Iinclude examples/modules/dnsresolver.c -o dnsresolver`.
`tests/test_dns_stack.c` checks actual end-to-end answer values, cache expiry,
AAAA, alias rejection, NXDOMAIN/REFUSED, callback failure and ID wraparound.
