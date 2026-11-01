# mulib-dnscache

A fixed-capacity positive cache of complete IN A/AAAA address sets. It caches
values rather than packet pointers, so neither packet nor input names survive
as borrowed storage. There is no allocation or system clock access.

## Integration and storage

Define `MULIB_IMPL_DNSCACHE` and dependency `MULIB_IMPL_DNS` once, then include
`mulib-dnscache.h`. C++ declarations have C linkage. No other implementation
switch is enabled by the header and no third-party dependency is required.

Allocate an array of `mulib_dnscache_entry_t`, then initialize a
`mulib_dnscache_t` with `init(&cache,slots,capacity)`. Initialization clears the
slots and checks capacity multiplication for overflow. NULL slots are valid
only at zero capacity. The cache borrows this array until discarded or
reinitialized; it does not free it. Do not modify initialized live slots.
Synchronize all access, including reads, because expired lookups remove slots.

## Values, keys and TTL

`mulib_dnscache_result_t` is a freely copyable owned value with `type`, `ttl`,
`count`, and `addresses[8][16]`. Types are A or AAAA only. Count must be 1..8;
larger sets return `LIMIT`, never silently truncate. IPv4 occupies the first
four bytes of each address; the remaining bytes are not interpreted. The
resolver zeroes unused bytes, while direct callers control their copied value.

`put(cache,name,length,&value,now)` normalizes the key using DNS ASCII name rules;
case and a final dot do not distinguish keys. A and AAAA are separate keys.
The complete value and normalized key are copied. Existing keys are replaced.
For a new key, an unused/expired slot is preferred; otherwise the slot with
shortest remaining lifetime is evicted (first index wins ties). This is expiry
priority, not LRU. Capacity zero returns `LIMIT` for a positive-TTL insertion.

`get(cache,name,length,type,now,&out)` returns `OK` and a copy with **remaining**
TTL, or `NOTFOUND` without changing `out`. The API uses application-supplied
unsigned 64-bit seconds, normally monotonic. A record inserted at `t` with TTL
`d` is live exactly when `now >= t && now-t < d`. This subtraction-based rule
avoids overflow even near `UINT64_MAX`; timestamp rollover/backwards movement
invalidates newer slots rather than extending their lifetimes. Clock units and
epoch must be the same for every operation on a cache.

TTL zero removes the matching key and succeeds without storing a result; it
does not evict an unrelated key. `expire(cache,now)` removes all expired slots
and returns the number removed. Expiration is otherwise lazy. There is no
periodic task or destructor. Direct cache users may store any `uint32_t` TTL;
the resolver treats wire TTLs with the high bit set as zero per its own policy.

## Errors and boundaries

`OK` is 1 and errors are negative, shared with `mulib-dns`. `INVALID` covers
invalid pointers, keys, zero counts and size overflow; `UNSUPPORTED` means a
non-address type; `LIMIT` means too many addresses or no available capacity.
Argument validation precedes insertion. There is no allocator to inject failures
into; the only storage limit is caller-selected entry count and eight addresses
per entry. Initialized cache objects and entries must not be forged or mutated.

No negative caching, wildcard/suffix keys, CNAME entries, DNS packet caching,
network access, DNSSEC state, stale serving or refresh policy is implemented.
The caller decides whether data is trustworthy before inserting it.

## Example and tests

`cc -std=c11 -Iinclude examples/modules/dnscache.c -o dnscache` builds a runnable
copy-ownership and expiry example. `tests/test_dns_stack.c` checks normalization,
family isolation, remaining TTL, replacement, zero capacity, invalid capacities,
backwards clocks, zero TTL and timestamps near `UINT64_MAX`.
