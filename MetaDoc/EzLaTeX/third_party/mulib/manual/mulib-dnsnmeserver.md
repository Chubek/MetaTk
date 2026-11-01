# mulib-dnsnmeserver

Compatibility facade for the historical `dnsnmeserver` spelling. New code should
include [`mulib-nameserver.h`](mulib-nameserver.md) and use `mulib_nameserver_*`.
The misspelled module is intentionally useful, not an empty reserved header or
an implementation-switch alias.

## Integration

Define all four switches explicitly in exactly one implementation translation
unit:

```c
#define MULIB_IMPL_DNS
#define MULIB_IMPL_DNSCLIENT
#define MULIB_IMPL_NAMESERVER
#define MULIB_IMPL_DNSNMESERVER
#include "mulib-dnsnmeserver.h"
```

`MULIB_IMPL_DNSNMESERVER` implements only the two compatibility functions. It
does not define `MULIB_IMPL_NAMESERVER` or any transitive dependency macro.
Implementations may instead live in separate translation units. C++ callers
receive C-linkage declarations. No third-party library, socket API or heap
allocation is introduced by this facade.

## Public forwarding API

`mulib_dnsnmeserver_t` is a typedef for `mulib_nameserver_t`; the storage layouts
and pointer types are identical.

* `mulib_dnsnmeserver_init(server,origin,length,records,count)` is an actual
  public C function forwarding to `mulib_nameserver_init`.
* `mulib_dnsnmeserver_answer(server,query,query_length,reply,reply_capacity,
  &written)` is an actual function forwarding to `mulib_nameserver_answer`.

Both functions preserve all return values, ownership rules and validation.
Their addresses can be taken as function pointers; they are not preprocessor
renamings. Code may mix canonical and compatibility calls on the same object.
There is no extra state, buffering or destructor.

## Ownership, limits and exact scope

Initialization owns only a copied canonical origin. Records and their RDATA
remain borrowed and immutable, with at most 1023 IN records inside one zone.
The supported types are A/AAAA/NS/CNAME/PTR/MX/TXT; CNAME owners are exclusive.
Names are restricted ASCII labels, with <=63-byte labels and <=255-byte expanded
wire names. Input/output packets must not overlap. Reply capacity is capped at
65535; the header/question must fit. Omitted RRs set TC without emitting partial
records. `written` changes only on success, while failed calls may alter output
storage. No object frees caller memory.

Single-question standard queries receive AA positive/NODATA/NXDOMAIN replies
inside the zone or REFUSED outside it. There is no recursion, DNSSEC, EDNS, SOA
proof, wildcard, zone transfer, socket server or automatic dependency setup.
Unsupported inputs return negative DNS status codes rather than being silently
ignored. Read the canonical nameserver manual for all behavior and invariants;
the compatibility spelling is not a separate protocol implementation.

## Example and verification

`cc -std=c11 -Iinclude examples/modules/dnsnmeserver.c -o dnsnmeserver` builds an
assert-based example using only the facade to initialize and answer NXDOMAIN.
`tests/test_dns_stack.c` exercises both forwarding functions alongside the
canonical nameserver and callback resolver. Declaration-only C++ compilation
does not require native networking headers.
