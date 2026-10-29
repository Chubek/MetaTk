# mulib-atn — epsilon/range transition networks

`include/mulib-atn.h` implements a finite nondeterministic automaton over
`uint32_t` tokens. Edges either consume no input (epsilon) or one token in an
inclusive `[lower, upper]` range. Multiple accepting states, nondeterministic
branches, parallel edges, and epsilon cycles are supported. Matching is always
of the **whole input**, not a search or a longest-prefix operation.

This module does not implement recursive rule calls, a lexer specification
language, regular-expression syntax, or Unicode decoding. `mulib-glr` uses it
as the finite LR-item transition network while compiling a CFG.

## Integration and ownership

ISO C11; declaration-only inclusion is also C++17-compatible. In exactly one
translation unit define all three switches **before any MuLIB include**:

```c
#define MULIB_IMPL_VEC
#define MULIB_IMPL_BITSET
#define MULIB_IMPL_ATN
#include "mulib-atn.h"
```

Dependencies: `mulib-vec.h`, `mulib-bitset.h`; no external libraries. Headers do
not turn on dependency implementations automatically. Allocation uses the C
standard library. `create` returns an owning pointer or NULL; `destroy(NULL)`
is safe. The graph copies all inserted records and never owns input tokens or
caller-provided bitsets. It is not internally synchronized; independent or
immutable graphs may be used concurrently.

## API

All IDs are zero-based `size_t` values. `MULIB_ATN_NONE` (`SIZE_MAX`) is the
missing-edge sentinel. State and edge IDs never change during graph growth.

| Function | Contract |
| --- | --- |
| `mulib_atn_create(max_states, max_edges)` | Initially empty graph; each zero limit selects its default. |
| `mulib_atn_destroy(a)` | Free graph and its records. |
| `mulib_atn_add_state(a, accepting, &id)` | Append a state; any nonzero flag means accepting. |
| `mulib_atn_set_accepting(a, state, flag)` | Change only the accepting flag. |
| `mulib_atn_add_epsilon(a, from, to, &id)` | Append a non-consuming edge between existing states. |
| `mulib_atn_add_range(a, from, to, lower, upper, &id)` | Append a consuming edge; `lower <= upper` is required. Both zero and `UINT32_MAX` are ordinary tokens. |
| `mulib_atn_state(a, id, &record)` | Copy `{accepting, first_edge}`. |
| `mulib_atn_edge(a, id, &record)` | Copy `{from, to, next, lower, upper, epsilon}`. Iterate from `first_edge` through `next` until NONE. Newest edges come first. |
| `mulib_atn_state_count(a)`, `mulib_atn_edge_count(a)` | Counts, or zero for NULL. |
| `mulib_atn_closure(a, &set, max_steps, &work)` | Expand a caller-owned state bitset in place by all epsilon paths. |
| `mulib_atn_step(a, &from, token, &to, max_steps, &work)` | Move a closed input set by one token, then close the output set. |
| `mulib_atn_match(a, start, tokens, length, max_steps, &accepted, &work)` | Full-input recognition from a single start state. |

Every `out_id` is required. On insertion failure the graph is unchanged and the
ID output is untouched. Identical transitions are intentionally **not**
interned. Query output records are copies, not borrowed pointers.

Subset operations require initialized `mulib_bitset_t` objects whose `bits`
equal the current state count. Adding a graph state therefore requires new or
resized caller sets. `step` requires distinct input/output backing storage,
clears the output, and assumes the input has already had epsilon closure.
Initialize a set with `mulib_bitset_init`, seed it with `mulib_bitset_set`, and
free it with `mulib_bitset_free`. The optional work pointer may be NULL.

`match` allocates its own sets and work queue. NULL tokens is valid only when
length is zero. Empty input succeeds precisely when an accepting state is
epsilon-reachable from `start`. The `accepted` output is required and is zero
on any error. A normal nonmatch is not an error.

## Errors and limits

`MULIB_ATN_OK = 0`, `INVALID = -1` (bad pointer, ID, range, or set shape),
`NOMEM = -2`, and `LIMIT = -3`. Allocation and size arithmetic are checked.
After an unsuccessful `closure` or `step`, the output set may be partially
expanded; discard or reinitialize it. Invalid-argument failures do not clear
the set. The graph itself is never changed by recognition or subset operations.

Defaults: **65536 states**, **262144 edges**, and **10000000 work units** per
public matching/subset operation. Zero selects the corresponding default, not
unlimited. A work unit is one state-slot inspection or outgoing-edge inspection.
Closure first inspects all state slots, then inspects each outgoing edge of
every reached state once. A step does its move scan and then a closure scan;
matching shares one budget over initial closure, all steps, and acceptance
checking. `work`, when supplied, reports the units spent even on an error.
Bitset zeroing, vector allocation, and copies are not individually metered.

Epsilon cycles terminate because states are marked when enqueued. Memory for
matching is O(states); a token step is O(states + inspected outgoing edges).
Entry limits bound logical entries, not allocator metadata or exact bytes;
vector capacities may be larger than current lengths.

## Runnable example

This accepts one decimal digit after a cyclic epsilon prefix:

```c
#define MULIB_IMPL_VEC
#define MULIB_IMPL_BITSET
#define MULIB_IMPL_ATN
#include "mulib-atn.h"
#include <assert.h>

int main(void) {
    mulib_atn_t *a = mulib_atn_create(0, 0);
    size_t s, q, end, e;
    uint32_t token = '7';
    int accepted;
    assert(a);
    assert(!mulib_atn_add_state(a, 0, &s));
    assert(!mulib_atn_add_state(a, 0, &q));
    assert(!mulib_atn_add_state(a, 1, &end));
    assert(!mulib_atn_add_epsilon(a, s, q, &e));
    assert(!mulib_atn_add_epsilon(a, q, s, &e));
    assert(!mulib_atn_add_range(a, q, end, '0', '9', &e));
    assert(!mulib_atn_match(a, s, &token, 1, 0, &accepted, NULL));
    assert(accepted);
    mulib_atn_destroy(a);
    return 0;
}
```

The standalone [`examples/modules/atn.c`](../examples/modules/atn.c) includes
positive, negative, and empty-input checks. From the repository root:

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/atn.c -o ./atn
./atn
```
