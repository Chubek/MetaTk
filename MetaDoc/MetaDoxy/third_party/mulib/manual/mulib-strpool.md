# mulib-strpool

Binary-safe, owning string interning with chained FNV-1a hash buckets. It
dogfoods `mulib-hash`, `mulib-alloc`, and `mulib-port`. Define implementation
switches for those modules and `MULIB_IMPL_STRPOOL` exactly once.

## API

`create(allocator)` copies callbacks and returns an owned pool. `intern(pool,s)`
accepts a C string; `intern_n(pool,s,n)` accepts bytes, including embedded NUL.
The latter permits NULL for an empty string. The returned borrowed pointer has
a convenience trailing NUL. Identical bytes and lengths return identical
pointers, even after bucket growth. Different lengths remain distinct.

`find_n` looks up without insertion. `reserve(pool,count)` ensures capacity
for count unique strings at a maximum 75% load. Expected lookup is O(n) for
hashing n bytes plus bucket search; FNV-1a is not collision-attack resistant.
All allocations and counts are overflow checked. Failure returns NULL/0 and
does not invalidate existing interned pointers.

`size` counts unique strings; `bytes` sums their lengths, excluding convenience
NULs and overhead. `reset` releases strings, invalidating all borrowed pointers
but retaining buckets. `destroy` releases the entire pool. No individual
deletion or reference counting; use separate pools for independent lifetimes.
Pools are not thread-safe.

## Example

```c
#define MULIB_IMPL_PORT
#define MULIB_IMPL_ALLOC
#define MULIB_IMPL_HASH
#define MULIB_IMPL_STRPOOL
#include <mulib-strpool.h>
#include <assert.h>
int main(void) {
    mulib_strpool_t *p = mulib_strpool_create(NULL);
    const char *a;
    assert(p);
    a = mulib_strpool_intern(p, "name");
    assert(a && a == mulib_strpool_intern(p, "name"));
    mulib_strpool_destroy(p);
    return 0;
}
```

Runnable example: `examples/modules/strpool.c`.
