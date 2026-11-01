# mulib-sppf — shared packed parse forests

`include/mulib-sppf.h` represents many derivation trees using shared symbol
nodes and packed alternatives. It supports nullable spans and cycles, including
finite graphs representing infinitely many finite derivation trees.

The representation is an **n-ary, symbol-node SPPF**: one packed alternative
stores a production ID and an ordered list of all RHS children. It is not
binarized and does not introduce intermediate grammar-slot nodes. In particular,
there is no claim of a cubic forest-size bound for arbitrary long productions.
The module has no grammar, semantic values/actions, disambiguation policy, or
automatic tree enumerator. `mulib-glr` supplies the grammar and builds this
forest while following all viable LR branches.

## Integration and ownership

ISO C11; C++17-compatible declarations. In one translation unit:

```c
#define MULIB_IMPL_VEC
#define MULIB_IMPL_SPPF
#include "mulib-sppf.h"
```

`mulib-vec` is the only module dependency. There are no external libraries or
implicit implementation switches. The forest owns copied node, alternative,
and child-ID records, never the objects denoted by application symbols or
production IDs. `destroy(NULL)` and `clear(NULL)` are safe. Read-only use may be
concurrent; mutation is not internally synchronized.

## Keys and span invariants

* Symbol nodes share **(kind, symbol ID, begin, end)**. Spans are half-open token
  intervals `[begin, end)`. Terminals cover exactly one token; nonterminals can
  cover an empty interval. Symbol IDs are uninterpreted `size_t` values.
* Packed alternatives share **(parent node, production ID, ordered child IDs)**.
  A production ID is an application label; using two different IDs for the same
  RHS intentionally represents two choices.
* Only nonterminals can have alternatives. Children must already exist, begin
  at the parent's beginning, concatenate without gaps or overlap, and end at
  the parent's end. An empty alternative requires an empty parent span.
* A child may be its own parent or create an indirect cycle. Child IDs must be
  valid, but no acyclicity or productivity restriction is imposed.

IDs are zero-based `size_t`; `MULIB_SPPF_NONE` (`SIZE_MAX`) marks an absent
alternative. No actual token payload is stored. Retain the original token
array if terminals need semantic values, particularly for range terminals.

## API

| Function | Contract |
| --- | --- |
| `mulib_sppf_create(max_nodes, max_alternatives, max_children)` | Owning empty forest, or NULL on allocation failure. Each zero limit selects its default. |
| `mulib_sppf_destroy(f)` | Release all forest storage. |
| `mulib_sppf_clear(f)` | Clear logical entries and invalidate every ID, retaining capacity. |
| `mulib_sppf_intern(f, kind, symbol, begin, end, &id, &created)` | Find/create a TERMINAL or NONTERMINAL symbol node. |
| `mulib_sppf_pack(f, parent, production, children, count, &id, &created)` | Find/create an alternative; copies child IDs. NULL children is allowed for count zero. |
| `mulib_sppf_node(f, id, &node)` | Copy `{kind, symbol, begin, end, first_alternative}`. |
| `mulib_sppf_alternative(f, id, &alternative)` | Copy `{parent, production, child_count, next}`. |
| `mulib_sppf_child(f, alternative, index, &id)` | Read an ordered child ID. |
| `mulib_sppf_node_count(f)`, `mulib_sppf_alternative_count(f)` | Counts, or zero for NULL. |
| `mulib_sppf_count(f, root, cap, max_steps, &count)` | Count derivation trees, distinguishing productive cycles and finite overflow. |

ID outputs are required; `created` may be NULL. It is one only for new entries,
and zero otherwise, including failure. Insertions are transactional for logical
entries; failed allocation can grow capacity but does not add a partial
alternative. IDs remain stable across all successful insertions. At a resource
limit, a request for an already interned key still succeeds.

Alternatives are prepended. Iterate `node.first_alternative`, following each
`alternative.next` until NONE. All query records are copies. Do not recursively
walk a potentially cyclic forest without a visited set or explicit tree-depth
limit. Destroying the forest itself is iterative and does not walk derivations.

## Counting and cyclic ambiguity

`count` counts **finite derivation trees**, not merely packed alternatives.
A terminal counts as one; an empty alternative counts as one; choices add;
ordered child combinations multiply. A node with no productive alternative
counts as zero.

The iterative algorithm first computes productivity, then reachability through
productive alternatives, then evaluates counts in dependency order. Any
remaining productive reachable dependency cycle implies arbitrarily many
finite derivation trees, so it reports INFINITE. A dead self-cycle with no base
case counts zero. A dead alternative alongside a productive one is ignored,
not mistaken for infinite ambiguity. No C recursion is used.

`cap` must be positive, up to `SIZE_MAX`. Results are:

| Status | count output |
| --- | --- |
| `MULIB_SPPF_OK` (0) | Exact finite count, including zero. Exactly cap is still OK. |
| `MULIB_SPPF_INFINITE` (1) | cap, with infinitely many finite derivation trees. |
| `MULIB_SPPF_LIMIT` (-3) | cap for a finite count exceeding cap; **zero** for work exhaustion. |
| `MULIB_SPPF_NOMEM` (-2), `INVALID` (-1) | Zero, when an output pointer was supplied. |

Thus a positive cap distinguishes count saturation from work exhaustion.
INFINITE takes precedence over finite arithmetic saturation if the computation
finishes. Counting allocates O(symbol nodes + reachable nodes) temporary space.
It scans the whole forest for productivity, including nodes unrelated to root;
large unrelated subgraphs can therefore consume the work budget. The simple
fixed-point scans may require quadratically many visits. Count work units are
node-slot, alternative, and child inspections, not allocations or vector copies.

## Limits and errors

Defaults: **65536 symbol nodes**, **262144 alternatives**, **1048576 total
child-ID references**, and **10000000 count-work units**. Zero constructor limits
or `max_steps` select defaults, not unlimited. Vector capacity can exceed its
logical count. Interning is a linear node scan; alternative deduplication scans
that parent's alternatives and compares ordered children. These insertion scans
have no separate work budget.

Other operations return OK, INVALID, NOMEM, or LIMIT. INVALID covers absent
outputs, missing IDs, bad kinds/spans, terminal parents, and non-concatenating
children. Capacity arithmetic and allocation failures are checked. No parser
grammar consistency checks are possible at this layer.

## Runnable example

Two distinct productions share the same terminal subtree:

```c
#define MULIB_IMPL_VEC
#define MULIB_IMPL_SPPF
#include "mulib-sppf.h"
#include <assert.h>

int main(void) {
    mulib_sppf_t *f = mulib_sppf_create(0, 0, 0);
    size_t leaf, root, alternative, count;
    assert(f);
    assert(!mulib_sppf_intern(f, MULIB_SPPF_TERMINAL, 'a', 0, 1, &leaf, NULL));
    assert(!mulib_sppf_intern(f, MULIB_SPPF_NONTERMINAL, 'S', 0, 1, &root, NULL));
    assert(!mulib_sppf_pack(f, root, 0, &leaf, 1, &alternative, NULL));
    assert(!mulib_sppf_pack(f, root, 1, &leaf, 1, &alternative, NULL));
    assert(!mulib_sppf_count(f, root, SIZE_MAX, 0, &count) && count == 2);
    mulib_sppf_destroy(f);
    return 0;
}
```

See `examples/modules/sppf.c` for additional sharing assertions:

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/sppf.c -o ./sppf
./sppf
```
