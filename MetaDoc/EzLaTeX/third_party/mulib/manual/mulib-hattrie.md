# MuLIB HAT-style trie

- **Header:** `include/mulib-hattrie.h`
- **Implementation switches:** `MULIB_IMPL_HATTRIE`, **`MULIB_IMPL_HASH`**
- **Dependency:** `mulib-hash.h` (FNV-1a); ISO C11, C++17-compatible declarations.

## Honest algorithmic scope

A focused adaptive byte-key trie with compressed branching prefixes and genuine
hashed leaf buckets. It is not a renamed linear map or a compatibility layer for
any external HAT-trie implementation. Keys are arbitrary byte strings (embedded
NUL, empty, non-UTF-8); values are copied `uint64_t`. Case and every byte matter.

Each leaf bucket has 32 hash slots with collision chains. A bucket exceeding 16
entries is burst into a branch: the longest common remaining prefix is compressed
into one byte string, a terminal entry handles a key ending at the branch, and
entries are distributed into child hash buckets by their next byte. Branches
have 256 child pointers. This simple focused implementation favors auditability
over dense-node memory optimization. Long shared prefixes do not cause one node
or recursive call per byte. Inserting a divergent or shorter key splits a
compressed prefix transactionally.

Exact lookup hashes the full key once and follows matching compressed branches,
then checks just the selected bucket chain. Expected lookup/update is O(key
length plus bucket collisions); FNV-1a is **not cryptographic**, so adversarial
collisions may degrade performance. Prefix enumeration first finds the relevant
subtree, then visits only it (filtering the final hash bucket as needed). Output
is **unordered**, not lexicographic. Deletion prunes empty buckets/branches but
does not merge nonempty one-child branches or shrink them back into buckets.

## Integration, ownership and errors

Define both implementation switches before any includes, in exactly one C
translation unit. The trie never defines the hash implementation switch for you.
Initialize with `mulib_hattrie_init` or `{0}` and finish with `mulib_hattrie_free`.
Free, enumeration and deletion use iterative parent-linked walks, not unbounded
recursive stack space. NULL/already-empty free is safe. Do not shallow-copy a
live trie or modify its `root`/`size` fields.

Keys are copied at insertion and released on deletion/free. Values are copied
integers. A NULL key/prefix is accepted only when its length is zero. Key memory
passed into the API need only remain valid for that call. A callback key is
borrowed during that callback and has **no guaranteed terminating NUL**. Callbacks
must not mutate the trie. There is no thread synchronization or `errno` contract.

An unsuccessful insertion preserves every mapping. Optional bursting may fail
after a new mapping was successfully inserted; in that case insertion returns 1
and the existing hash bucket remains intact, possibly above its target size.
Later insertion retries the burst. Buckets have collision chains, so exceeding
the threshold does not cause capacity corruption. Mandatory branch splits are
allocated completely before any old structure changes.

## API

```c
void mulib_hattrie_init(mulib_hattrie_t *trie);
int mulib_hattrie_set(mulib_hattrie_t *trie, const void *key, size_t length,
                     uint64_t value);
int mulib_hattrie_get(const mulib_hattrie_t *trie, const void *key, size_t length,
                     uint64_t *value);
int mulib_hattrie_remove(mulib_hattrie_t *trie, const void *key, size_t length,
                        uint64_t *old_value);
size_t mulib_hattrie_prefix(const mulib_hattrie_t *trie, const void *prefix,
                           size_t length, mulib_hattrie_visit_fn visit, void *ctx);
int mulib_hattrie_validate(const mulib_hattrie_t *trie);
void mulib_hattrie_free(mulib_hattrie_t *trie);
```

- `set`: 1 succeeds (duplicate keys replace without changing size), 0 indicates
  invalid input, ownership allocation failure or size/allocation overflow.
- `get`/`remove`: 1 if found, 0 otherwise; optional output stays unchanged on a
  miss. `remove` allocates nothing.
- `prefix`: callback `int(const void *,size_t,uint64_t,void *)` receives matching
  keys, including an exact prefix key. Return nonzero to continue or zero to stop.
  The return counts callbacks, including stop; NULL callback/invalid input yields
  zero. Empty prefix enumerates the whole trie.
- `validate`: diagnostic check of live objects: parent/edge relationships, hash
  slots and cached hashes, reachability of each key through the compressed trie,
  bucket counts and total size. It is not a validator for forged C pointers.

## Runnable example

```c
#include <assert.h>
#define MULIB_IMPL_HASH
#define MULIB_IMPL_HATTRIE
#include "mulib-hattrie.h"
int main(void) {
    mulib_hattrie_t trie;
    uint64_t value;
    const unsigned char binary_key[] = {'a', 0, 'b'};
    mulib_hattrie_init(&trie);
    assert(mulib_hattrie_set(&trie, binary_key, sizeof(binary_key), 42));
    assert(mulib_hattrie_set(&trie, NULL, 0, 7));
    assert(mulib_hattrie_get(&trie, binary_key, sizeof(binary_key), &value));
    assert(value == 42 && mulib_hattrie_validate(&trie));
    assert(mulib_hattrie_remove(&trie, NULL, 0, NULL));
    mulib_hattrie_free(&trie);
    return 0;
}
```

For bursting and prefix enumeration:

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/hattrie.c -o ./hattrie
./hattrie
```

`tests/test_indexes.c` exercises randomized differential lookup/delete/update and
prefix enumeration, empty/binary keys, exact-prefix terminals, 16 KiB compressed
prefixes, divergent/shorter-key splits, deletion to empty, injected split OOM and
deferred bursting. No external libraries are used.
