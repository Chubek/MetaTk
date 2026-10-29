# MuLIB AVL

- **Header:** `include/mulib-avl.h`
- **Implementation switch:** `MULIB_IMPL_AVL`
- **Dependencies:** ISO C11 standard library only; declaration-only C++17 compatible.

## Scope and algorithm

An owning ordered map from `int64_t` keys to copied `uint64_t` values. This is an
actual AVL tree, not a linear dictionary: every node stores its subtree height,
and insertion/deletion repair height imbalances with single or double rotations.
Keys are unique. Setting an existing key replaces its value without allocating or
changing `size`. Both signed key extremes and all unsigned values are supported.

Lookup, insertion and deletion take O(log n) time; ascending traversal and release
take O(n). The implementation uses O(log n) recursive stack space and one heap
allocation per record. There is no generic comparator, rank/select API, duplicate
multimap behavior, persistent storage, locking, or implicit value destructor.

## Integration and ownership

Define `MULIB_IMPL_AVL` before including the header in exactly one C translation
unit. Other translation units include it without the switch. The implementation
is outside the declaration guard, with a separate implemented guard.

Initialize `mulib_avl_t` with `mulib_avl_init` or `{0}`. It exposes `root` (an
opaque node pointer) and `size` for inspection; do not modify either. The map owns
all nodes, and values are plain integers with no external lifetime. Do not
shallow-copy a live map, reinitialize a live map without freeing it, or mutate
the map from a traversal callback. Call `mulib_avl_free` to release and reset it;
repeated free and free of NULL are safe. No API sets `errno`.

## API

```c
void mulib_avl_init(mulib_avl_t *tree);
int mulib_avl_set(mulib_avl_t *tree, int64_t key, uint64_t value);
int mulib_avl_get(const mulib_avl_t *tree, int64_t key, uint64_t *value);
int mulib_avl_remove(mulib_avl_t *tree, int64_t key, uint64_t *old_value);
size_t mulib_avl_foreach(const mulib_avl_t *tree,
                       mulib_avl_visit_fn visit, void *context);
int mulib_avl_validate(const mulib_avl_t *tree);
void mulib_avl_free(mulib_avl_t *tree);
```

- `set` returns 1 on success, 0 for NULL tree, size overflow or allocation
  failure. Failure leaves all keys, values and size unchanged. Replacements work
  even when an allocator cannot allocate new nodes.
- `get` and `remove` return 1 if found, 0 otherwise. Their optional output is
  unchanged on a miss. `remove` allocates nothing, handles zero/one/two-child
  deletion, and rebalances every affected ancestor.
- `foreach` invokes `int visit(int64_t,uint64_t,void *)` in ascending key order.
  Return nonzero to continue or zero to stop. The result counts callbacks,
  including a callback that stops; NULL tree/callback yields zero.
- `validate` returns 1 when strict key order, stored heights, AVL balance and
  record count agree. It is O(n), intended for diagnostics/tests of live API
  objects, not for dereferencing arbitrary corrupted pointers.

## Runnable example

```c
#include <assert.h>
#define MULIB_IMPL_AVL
#include "mulib-avl.h"
int main(void) {
    mulib_avl_t tree;
    uint64_t value;
    mulib_avl_init(&tree);
    for (int64_t i = 999; i >= 0; --i)
        assert(mulib_avl_set(&tree, i, (uint64_t)i * 2));
    assert(mulib_avl_get(&tree, 42, &value) && value == 84);
    assert(mulib_avl_remove(&tree, 42, &value) && value == 84);
    assert(mulib_avl_validate(&tree));
    mulib_avl_free(&tree);
    return 0;
}
```

Build the fuller traversal example with:

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/avl.c -o ./avl
./avl
```

`tests/test_indexes.c` compares random operations and ordered traversal against
an independent reference array. It also checks ascending/descending adversarial
insertion/deletion, duplicate replacement, empty trees, key extremes, early stop
and allocation-failure preservation.
