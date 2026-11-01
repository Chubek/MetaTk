# mulib-gss — graph-structured stacks

`include/mulib-gss.h` stores shared stack tops and labelled predecessor edges.
A node is interned by **(state, input position)**. An edge is interned by
**(head node, predecessor node, label)**. Distinct histories can consequently
share a top, a tail, or both, without copying complete stacks. `mulib-glr` uses
labels that identify `mulib-sppf` symbol nodes.

The container itself does not perform parsing, pop paths, interpret labels,
collect unreachable entries, or enforce a DAG. Same-position edges and cycles
are intentional: they represent nullable/cyclic CFG derivations. Clients must
use visited keys or a finite path length when traversing them.

## Integration and ownership

ISO C11, with C++17-compatible declarations. Define both implementation switches
before any MuLIB include in one translation unit:

```c
#define MULIB_IMPL_VEC
#define MULIB_IMPL_GSS
#include "mulib-gss.h"
```

The only module dependency is `mulib-vec`. There are no external libraries or
implicit dependency switches. The graph owns its copied node/edge records;
labels are opaque integers and are **not** owned objects. `destroy(NULL)` and
`clear(NULL)` are safe. The mutable graph is not internally synchronized.

## API and sharing

| Function | Contract |
| --- | --- |
| `mulib_gss_create(max_nodes, max_edges)` | Owning empty graph or NULL on allocation failure. Each zero limit selects a default. |
| `mulib_gss_destroy(g)` | Release graph storage, not anything represented by labels. |
| `mulib_gss_clear(g)` | Drop all entries and invalidate all IDs, retaining capacity. |
| `mulib_gss_intern(g, state, position, &id, &created)` | Return the existing key or append a node. |
| `mulib_gss_link(g, head, predecessor, label, &id, &created)` | Return the existing edge or append it. Both endpoints must exist. |
| `mulib_gss_node(g, id, &node)` | Copy `{state, position, first_edge}`. |
| `mulib_gss_edge(g, id, &edge)` | Copy `{head, predecessor, label, next}`. |
| `mulib_gss_node_count(g)`, `mulib_gss_edge_count(g)` | Entry counts; zero for NULL. |

IDs are zero-based `size_t` values and remain stable across growth.
`MULIB_GSS_NONE` is `SIZE_MAX`. `state` is an uninterpreted integer, including
zero; positions are also `size_t`. The predecessor position must be less than
or equal to the head position. Labels may be any integer **except NONE**.
Positions and labels are not checked against an external token buffer/forest.

Insertion requires an ID output. The optional `created` pointer receives one
only for a new entry and zero otherwise, including on errors. Existing entries
are found even if the graph has reached its limit. Failure leaves logical
entries unchanged and does not write the ID output.

Iterate a top's outgoing predecessor edges using `node.first_edge`, then each
`edge.next`, until NONE. Entries are prepended, so newest edges come first.
Records returned by the query functions are copies. Growth never invalidates
IDs, but a previously copied `first_edge` will not include subsequently added
edges; restart iteration to see them. No internal storage pointer is exposed.

## Errors and resource bounds

Statuses are `MULIB_GSS_OK = 0`, `INVALID = -1`, `NOMEM = -2`, and `LIMIT = -3`.
Bad IDs, absent output pointers, a NONE label, and forward-in-input predecessor
edges are invalid. Size multiplication and entry growth are checked.

Default limits are **65536 nodes** and **262144 edges**. Zero means default,
not unlimited. Limits bound logical counts rather than exact allocation bytes;
`vec` uses amortized capacity growth. Interning scans all nodes linearly;
linking scans the head's outgoing edges linearly. These scans do not have a
separate work budget. No traversal is performed implicitly, so graph cycles
cannot make an insertion or query loop indefinitely.

## Runnable example

Two states share the same tail and merge at one top:

```c
#define MULIB_IMPL_VEC
#define MULIB_IMPL_GSS
#include "mulib-gss.h"
#include <assert.h>

int main(void) {
    mulib_gss_t *g = mulib_gss_create(0, 0);
    size_t base, left, right, top, edge, again;
    int created;
    assert(g);
    assert(!mulib_gss_intern(g, 0, 0, &base, NULL));
    assert(!mulib_gss_intern(g, 1, 1, &left, NULL));
    assert(!mulib_gss_intern(g, 2, 1, &right, NULL));
    assert(!mulib_gss_intern(g, 3, 2, &top, NULL));
    assert(!mulib_gss_link(g, left, base, 10, &edge, NULL));
    assert(!mulib_gss_link(g, right, base, 11, &edge, NULL));
    assert(!mulib_gss_link(g, top, left, 12, &edge, NULL));
    assert(!mulib_gss_link(g, top, right, 12, &edge, NULL));
    assert(!mulib_gss_link(g, top, right, 12, &again, &created));
    assert(!created && edge == again);
    assert(mulib_gss_node_count(g) == 4);
    assert(mulib_gss_edge_count(g) == 4);
    mulib_gss_destroy(g);
    return 0;
}
```

The equivalent standalone example is `examples/modules/gss.c`:

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/gss.c -o ./gss
./gss
```
