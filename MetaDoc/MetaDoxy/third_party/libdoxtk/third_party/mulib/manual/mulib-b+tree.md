# MuLIB B+ tree

- **Header:** `include/mulib-b+tree.h`
- **Implementation switch:** `MULIB_IMPL_BPTREE`
- **Dependencies:** ISO C11 standard library only; declarations support C++17.

## Scope and representation

This is an in-memory, order-eight B+ tree mapping unique `int64_t` keys to copied
`uint64_t` values. All records live in sorted leaf arrays; leaves have reciprocal
next/previous links for range scans. Internal nodes contain only child pointers
and routing minima, not records. Each internal routing key is the minimum of its
corresponding child (including child zero).

Nonroot nodes have 4–8 records (leaves) or children (internal nodes). An internal
root has 2–8 children; a leaf root has 1–8 records. Empty trees have no root or
first leaf. Overflow at nine items splits 4/5 and may propagate to a new root.
Deletion borrows from a sibling with spare occupancy or merges siblings, repairs
routing minima, and collapses a one-child internal root. Leaves all have equal
depth. These invariants are maintained after every successful mutation.

Lookup/update/delete take O(log n), as does finding a range's first record.
Subsequent range traversal follows the linked leaves and costs O(k) for k
visited records. Order eight is fixed, not a configurable disk page size.
There is no on-disk format, concurrent mutation, generic comparator, cursor
lifetime guarantee, or multimap support. All integer key/value extremes work.

## Integration, failure and ownership

Define `MULIB_IMPL_BPTREE` before the header in exactly one C translation unit.
Initialize `mulib_bptree_t` using `mulib_bptree_init` or `{0}`. Its public `root`,
`first` and `size` fields are read-only to callers. Nodes belong exclusively to
the tree; do not shallow-copy it. Values are copied integers, not owned pointers.
Free resets all fields and is safe on NULL or an already-freed tree.

Insertion determines the exact split chain and allocates all required nodes
before modifying any live node. Allocation failure or size overflow leaves the
tree unchanged. Replacing a duplicate key and deleting any key allocate nothing.
Errors do not imply any particular `errno`.

## API

```c
void mulib_bptree_init(mulib_bptree_t *tree);
int mulib_bptree_set(mulib_bptree_t *tree, int64_t key, uint64_t value);
int mulib_bptree_get(const mulib_bptree_t *tree, int64_t key, uint64_t *value);
int mulib_bptree_remove(mulib_bptree_t *tree, int64_t key, uint64_t *old_value);
size_t mulib_bptree_range(const mulib_bptree_t *tree, int64_t low, int64_t high,
                         mulib_bptree_visit_fn visit, void *context);
int mulib_bptree_validate(const mulib_bptree_t *tree);
void mulib_bptree_free(mulib_bptree_t *tree);
```

- `set`: 1 on insert/replacement; 0 for NULL tree, OOM or size overflow.
- `get`/`remove`: 1 if found, 0 for a miss or NULL tree. Optional output is
  unchanged on failure. Replacement leaves `size` unchanged.
- `range`: visits the **inclusive** interval `[low,high]` in ascending order.
  A full traversal uses `INT64_MIN, INT64_MAX`, without overflow-prone arithmetic.
  Callback signature is `int(int64_t,uint64_t,void *)`; nonzero continues, zero
  stops. The result counts callbacks including the stopping callback. Reversed
  bounds, NULL tree, or NULL callback produce zero. Do not mutate from callbacks.
- `validate`: O(n) diagnostic check of occupancy, parents, strict order, routing
  minima, equal leaf depth, the entire linked leaf chain, first pointer and size.
  Supply a live API object, not arbitrary fabricated pointers.

## Runnable example

```c
#include <assert.h>
#define MULIB_IMPL_BPTREE
#include "mulib-b+tree.h"
static int count(int64_t key, uint64_t value, void *ctx) {
    size_t *n = (size_t *)ctx;
    assert(value == (uint64_t)key);
    ++*n;
    return 1;
}
int main(void) {
    mulib_bptree_t tree;
    size_t n = 0;
    mulib_bptree_init(&tree);
    for (int64_t i = 0; i < 1000; ++i)
        assert(mulib_bptree_set(&tree, i, (uint64_t)i));
    assert(mulib_bptree_range(&tree, 40, 50, count, &n) == 11 && n == 11);
    assert(mulib_bptree_remove(&tree, 45, NULL));
    assert(mulib_bptree_validate(&tree));
    mulib_bptree_free(&tree);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude 'examples/modules/b+tree.c' -o ./bptree
./bptree
```

The shared `tests/test_indexes.c` executable checks random differential behavior,
duplicates, empty trees, range ordering, signed extremes, invariant preservation
under many ascending/descending inserts/removes, and OOM at each allocation of
a leaf/internal/root split chain.
