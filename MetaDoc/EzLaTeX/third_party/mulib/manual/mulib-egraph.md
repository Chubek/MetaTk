# mulib-egraph

Bounded binary equality graphs with real hash-consing, disjoint-set unions,
congruence rebuilding, and minimum-additive-cost expression extraction. This
is an untyped term container, not a solver for arithmetic equalities or types.

## Integration and storage

Include `mulib-egraph.h`, define `MULIB_IMPL_EGRAPH` exactly once. There are no
implementation dependencies. The graph type is opaque; C++ declarations have
C linkage. No concurrent graph mutation is supported.

Set `mulib_egraph_t *g = NULL` and call `mulib_egraph_create(node_limit,&g)`.
Storage is fixed for the graph lifetime: enodes, parent IDs and an open-addressed
hash table. There are no allocations in add, find, union or rebuild. Destroy
with `mulib_egraph_destroy` (NULL is harmless). Never duplicate ownership.
Zero capacity is valid but every new node returns RESOURCE_LIMIT.

Each new enode initially creates a class with the same numeric ID as its stable
enode index. `mulib_egraph_count` counts all enodes, **including equivalent
alternatives**, not the number of canonical classes. Unions do not reclaim node
capacity. `mulib_egraph_find` returns the canonical representative, or NONE for
an invalid ID. IDs remain usable after unions. Find performs path compression;
unions deterministically retain the smaller representative.

## Hash-consing and congruence

An enode key is `(unsigned opcode, int64_t literal, arity, child classes)`.
Arity is 0/1/2. Literal participates in every key, not just constant terminals;
use zero for nonliteral operations by convention. Children are existing class
IDs; unused child fields are ignored and normalized to NONE.

`mulib_egraph_add(g,node,&id)` canonicalizes children and hash-conses the key.
An existing key returns its class without consuming capacity, even when the
graph is full. An invalid child/arity is INVALID. A new node on a full graph is
RESOURCE_LIMIT. Failure does not add a node.

`mulib_egraph_union(g,a,b,&changed)` asserts equality, without interpreting
opcodes or checking type/semantic validity. A nontrivial union increments the
revision and marks the graph **dirty**. While dirty, add and extraction return
DIRTY; find, node inspection and further unions remain valid. Repeated union
of equal classes is a no-op. `changed` is optional.

`mulib_egraph_rebuild(g,work_limit,&used)` repeatedly canonicalizes all children,
rebuilds the hash table, and unions congruent parents until stable. If a=b, then
f(a)=f(b), including arbitrarily many parent levels. One work unit is one enode
visit per pass. A clean graph requires no work. Limit failure leaves valid
unions but a dirty hash table; call rebuild again with enough budget to finish.
It restarts a pass rather than promising incremental per-node progress, so a
budget permanently below the enode count cannot complete a dirty graph.

`mulib_egraph_node(g,index,&node,&class_id)` returns a copy with canonical
children and containing class, even while dirty; either output is optional.
`mulib_egraph_revision` increments for each new node or nontrivial union,
including congruence unions, but not merely canonicalizing child fields.

## Extraction

Zero initialize `mulib_egraph_tree_t`. Call
`mulib_egraph_extract(g,root,cost_callback,user,{work_limit,node_limit},&tree)`.
The graph must be clean. The callback supplies a nonnegative `uint64_t` local
enode cost; NULL means unit cost. It must be deterministic/pure, must not mutate
the graph, and may be called more than once per enode. Total cost is local cost
plus every child cost, counting repeated uses independently.

Fixed-point dynamic programming finds a minimum representable additive cost.
Cyclic classes with finite alternatives are supported. Equal-cost alternatives
retain their first discovered choice, avoiding gratuitous zero-cost cycles.
Overflowing alternatives are skipped; OVERFLOW reports an unavailable root cost
that depends on overflow. CYCLE means no finite selected acyclic tree can be
formed. Graphs constructed solely by valid adds/unions retain finite structural
alternatives, even when their classes have cycles; cycle checks also defend the
extraction traversal rather than assuming expression graphs are trees.

Success owns a new **expanded postorder tree**: `tree.nodes`, `tree.count`,
`tree.root == count-1`, and `tree.cost`. Children index earlier output records,
not original eclasses. Release with `mulib_egraph_tree_free`. No input pointer
is retained. Failure frees all extraction scratch and preserves empty output.
The node limit bounds expanded output, which can be exponentially larger than
an e-graph DAG. The work limit counts DP enode visits and tree-frame visits;
zero is an actual zero. Traversal is iterative with explicit active-class cycle
checks, not recursive on attacker-chosen depth.

## Limits, complexity and composition

Create checks capacity, doubled hash sizing, power-of-two growth and all
allocation-byte products before allocating. Hash slots are a power of two at
least twice node capacity (at least two slots). Fixed allocations are one
enode and one parent per capacity slot plus one `size_t` per hash slot. The
capacity bound also makes the change counter representable. Allocation failure
is NO_MEMORY, size/count/work bounds are RESOURCE_LIMIT, invalid pointers/IDs
are INVALID. `mulib_egraph_status_string` returns static text.

Rebuild can need multiple whole-graph passes; every merging pass reduces class
count. Hash probing/path compression are not separately charged work units.
Extraction uses O(enodes) scratch, O(enodes²) worst-case DP enode visits and the
explicit output allocation `limits.nodes*sizeof(node)`. No allocator injection
API or global state is installed. The limits bound storage/algorithm visits,
not a hard wall-clock time under hash collisions or costly callbacks.

`mulib-rewrite` supplies bounded pattern matching and equality saturation on
this graph. For common MIR conversion, clients map opcodes/types and postorder
children explicitly. Only introduce equalities valid for the intended types,
overflow/fault semantics and evaluation model: asserting `x*0=0` may hide a
faulting x. Typed MIR itself does not become valid merely by using its opcode
numbers in an e-graph.

## Runnable example

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/egraph.c -o /tmp/egraph
/tmp/egraph
```

The example unions two terminals, proves equality of their parent terms through
rebuild, and extracts a two-node minimum-cost tree. Compiler IR tests cover
multilevel congruence, dirty/rebuild limits, cyclic classes with finite leaves,
zero costs, overflow, duplicate hash-consing and output limits.
