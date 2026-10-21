# Chapter 37 — Graphs, Terms and Reduction

`DSLtk.hpp` and `DSLtk.d` provide a directed multigraph, first-order terms,
term rewriting, and sharing-preserving term graph reduction. The feature tags
are `Graphs`, `Terms`, `TermRewriting`, and `GraphReduction`. As with
`EqualitySaturation`, the types and functions are also usable independently
of feature composition. C++ names live in `dsl`; D names are module-level.

## Directed graphs

In C++, construct `DirectedGraph<NodeValue, EdgeValue>`; both payload types
default to `std::string`. In D, use `new DirectedGraph!(NodeValue, EdgeValue)`;
both default to `string`. D graphs are reference types. C++ graphs are value
types. These mutable objects are not synchronized.

```cpp
DirectedGraph<> graph;
auto entry = graph.add_node("entry");
auto exit = graph.add_node("exit");
auto edge = graph.add_edge(entry, exit, "next");
auto path = graph.shortest_path(entry, exit);
assert(path && path->size() == 2);
std::string dot = graph.to_dot("Control flow");
```

`GraphNodeId` and `GraphEdgeId` are graph-local integer handles. Handles are
never recycled; removal invalidates only the removed handles. Using an invalid
handle throws. Handles from different graphs must not be mixed. Parallel edges,
self-loops, cycles, and isolated nodes are supported. Edge insertion order is
preserved in adjacency lists, including when an incoming edge is redirected.

| Operation | Behavior |
| --- | --- |
| `add_node(value)`, `add_edge(source, target, value)` | Return new stable handles; edge payload defaults to its type's default value. |
| `contains(id)`, `contains_edge(id)` | Test whether a handle is live. |
| `value(id)`, `set_value(id, value)`, `edge(id)` | Read or replace a node payload; read edge endpoints and payload. |
| `nodes()`, `edges()`, `node_count()`, `edge_count()` | Enumerate or count live entities, excluding removed entries. |
| `incoming(id)`, `outgoing(id)` | Return edge handles in adjacency order. |
| `predecessors(id)`, `successors(id)` | Return adjacent node handles; parallel edges produce repeated entries. |
| `remove_edge(id)`, `remove_node(id)` | Remove an edge, or a node and all its incident edges. |
| `redirect(from, to)` | Retarget every incoming edge of `from` to `to`, preserving edge IDs and payloads; keep both nodes. |
| `breadth_first(root)` | Visit each reachable node once in breadth-first order. |
| `depth_first(root, postorder = false)` | Iterative DFS, with optional child-before-parent order. Cycles terminate safely. |
| `shortest_path(source, target)` | Return an optional unweighted shortest path, including endpoints; absent if unreachable. |
| `topological_sort()` | Sort the entire graph, including disconnected nodes; throw if cyclic. |
| `strongly_connected_components()` | Partition all live nodes into strongly connected components. |

Traversals, shortest paths, topological sorting, and strongly connected
components take linear time in visited vertices and edges, apart from scanning
retired storage slots for whole-graph operations. Node and edge enumeration
and counts scan the corresponding storage. Deletion removes entries from
adjacency arrays. References returned by C++ accessors can be invalidated by
subsequent graph mutations; retain IDs rather than references across mutations.

## DOT output

`to_dot(name = "G")` returns complete directed Graphviz DOT text, including
isolated nodes and parallel edges. Internal IDs such as `n0` are used as DOT
identifiers; payloads become quoted labels. Quotes, backslashes, and line breaks
are escaped. Tabs are retained as literal tabs. Other ASCII control characters
are rejected. Output order is deterministic for the same mutation history.

The default renderer uses stream insertion in C++ and `to!string` in D.
For arbitrary payloads, pass label callbacks:

```cpp
auto dot = graph.to_dot(
    [](const std::string& value) { return "node: " + value; },
    [](const std::string& value) { return value; },
    "Annotated graph");
std::ofstream("graph.dot") << dot;
```

```d
import DSLtk;
import std.file : write;

auto graph = new DirectedGraph!();
auto entry = graph.add_node("entry");
auto exit = graph.add_node("exit");
graph.add_edge(entry, exit, "next");
write("graph.dot", graph.to_dot("Control flow"));
```

DOT generation has no external dependency. To turn the text into an image,
install Graphviz and run `dot -Tsvg graph.dot -o graph.svg`. The toolkit does
not launch Graphviz or write files implicitly.

## First-order terms

`Term` is a structural value with a `symbol`, ordered `arguments`, and
`is_variable`. Variables and nullary function symbols are distinct. Both
implementations copy terms deeply. D exposes `clone()` for copying a const term
into a mutable value.

```cpp
auto variable = Term::variable("x");
auto zero = Term::function("0");
auto sum = Term::function("add", {variable, zero});
assert(sum == parse_term("(add ?x 0)"));
```

D uses `Term.variable("x")` and `Term.function_("add", arguments)` because
`function` is a D keyword. `parse_term` uses the existing S-expression reader.
Unquoted `?name` atoms are variables; quoted `"?name"` atoms are literal
symbols. Function heads must be literal atoms. Empty lists, variable heads,
empty variable names, and variables with arguments are rejected. Symbols may
contain spaces or punctuation; dumping quotes them as needed. There is no
global signature table: the same function name can appear at different arities,
but matching and unification require both name and arity to agree.

`dump()` and `sexpr()` serialize terms; `term_from_sexpr()` converts the other
way. `variables()` returns a set of bare variable names (a `bool[string]` map
in D), `ground()` tests for absence of variables, and `size()` counts tree
nodes. `at(position)` selects a subterm using zero-based argument indices;
the empty position selects the root. `replaced(position, replacement)` returns
a new term and leaves the original unchanged. Invalid positions throw.

`TermSubstitution` maps bare variable names to terms. `match_term(pattern,
subject)` returns optional bindings and enforces structural equality at repeated
variables. Subject variables are treated as subject data, not as additional
pattern variables. A failed match publishes no partial bindings.

`substitute(term, bindings)` performs simultaneous substitution; absent
variables remain unchanged and inserted bindings are not recursively
substituted again. `unify_terms(left, right)` returns an optional most general
unifier with an occurs check. Its bindings are composed so one simultaneous
substitution makes both input terms equal. It rejects cyclic equations such
as `?x = (f ?x)`. Optional results use `std::optional` in C++ and `Nullable`
(`isNull` / `get`) in D.

## Term rewrite systems

`TermRule` contains `name`, `lhs`, and `rhs`. Rules require a nonvariable
left side and bind every right-side variable on the left. `TermRewriteSystem`
validates and owns copies of its rules. `add_rule` accepts a `TermRule` or an
S-expression string; `term_rule` parses a standalone rule. Supported forms are
`(rewrite name lhs rhs)`, `(= lhs rhs)`, and `(=> lhs rhs)`.

```cpp
TermRewriteSystem rules;
rules.add_rule("(rewrite add-zero (add ?x 0) ?x)");
rules.add_rule("(rewrite mul-one (mul ?x 1) ?x)");
auto report = rules.normalize(parse_term("(mul (add a 0) 1)"), 100);
assert(report.normal_form() && report.steps == 2);
assert(report.term == parse_term("a"));
```

`rewrite_once(term, strategy)` returns an optional `TermRewriteStep` containing
the entire rewritten `term`, `rule_index`, and redex `position`. Rules are tried
in insertion order at each position. `RewriteStrategy::Outermost` searches
root before children; `Innermost` searches children before root. Both visit
arguments from left to right. D uses enum access with `.` instead of `::`.

`normalize(term, max_steps = 1000, strategy = Outermost)` returns a
`TermReductionResult` with `term`, `steps`, and `stop`. `ReductionStop::NormalForm`
means no rule matches anywhere; `StepLimit` means another rewrite is available
but the budget is exhausted. A zero budget only checks reducibility. A term
that reaches normal form on the last allowed step reports `NormalForm`.
Identity rules count as rewrites and exhaust the budget rather than being
silently discarded. Termination and confluence are not assumed or proved.

## Shared term graphs and graph reduction

`TermGraph` stores ordered, acyclic term graphs using
`DirectedGraph<TermGraphLabel, size_t>`. Its graph view is read-only so the
term shape and acyclicity invariants cannot be broken by ordinary callers.
Nodes carry a symbol and variable flag; edges carry zero-based argument indices.

```cpp
TermRewriteSystem rules;
rules.add_rule("(= (add ?x 0) ?x)");
TermGraph graph;
auto root = graph.add("(pair (add a 0) (add a 0))");
auto report = graph.reduce(root, rules, 100);
assert(report.normal_form() && report.steps == 1);
assert(graph.to_term(report.root) == parse_term("(pair a a)"));
graph.collect_garbage({report.root});
std::ofstream("reduced.dot") << graph.to_dot();
```

```d
auto rules = new TermRewriteSystem;
rules.add_rule("(= (add ?x 0) ?x)");
auto graph = new TermGraph;
auto root = graph.add("(pair (add a 0) (add a 0))");
auto report = graph.reduce(root, rules, 100);
assert(report.normal_form() && report.steps == 1);
assert(graph.to_term(report.root) == parse_term("(pair a a)"));
graph.collect_garbage([report.root]);
```

`add(term_or_source, share = true)` reuses existing nodes with equal labels and
identical child handles. With `share = false`, each occurrence gets its own
nodes. `add_node(symbol, children)` explicitly constructs a function node with
existing children and never merges it. These construction operations cannot
introduce cycles. `graph()` exposes traversal and read APIs; `to_term(root)`
unfolds a graph into a tree; `to_dot(name = "TermGraph")` renders all live nodes.
Each sharing-enabled `add` builds a hash index of live nodes and interns the
input bottom-up. Rebuilding the index keeps it consistent with reductions and
edge redirections between additions.

Graph matching operates directly on node handles. Repeated variables compare
subgraphs structurally even when they are not physically shared. When a rule
fires, right-side variables reuse their bound nodes. Every incoming edge of the
redex is redirected to the replacement, so all occurrences of a shared redex
change in one reduction. The redex node is removed. Newly introduced function
nodes are fresh; rewriting does not automatically maximize sharing.

`rewrite_once(root, rules, strategy)` returns an optional `GraphRewriteStep`
with `root`, `redex`, `replacement`, and `rule_index`. `reduce(root, rules,
max_steps = 1000, strategy = Outermost)` returns `GraphReductionResult` with
`root`, `steps`, and `stop`. Only nodes reachable from the selected root are
searched. The same rule priority, strategy, and budget contracts apply as for
term rewriting, with each shared node visited once per search.

Always continue with the returned root: rewriting the root removes its old ID.
Other external root handles that equal a removed redex must also be updated
using the step's replacement. Incoming edges from other roots are redirected
too. Garbage is retained until `collect_garbage(roots)`; supply every root you
want to keep. Empty roots remove all nodes. Invalid roots throw before collection
removes anything. Remaining handles stay stable.

The general graph supports cycles; `TermGraph` deliberately represents finite
acyclic terms. Step budgets limit rewrites, not memory or recursion depth.
Tree operations recurse through terms, and unfolding a highly shared graph can
produce an exponentially larger tree. These APIs do not implement higher-order
binding, cyclic term reduction, completion, or equality saturation; the separate
`EGraph` API remains available for equality saturation.

## Verification

The graph and term regression tests cover both implementations. The standalone
test project includes D tests automatically when `ldc2`, `dmd`, or `gdc` is
available; use `-DDSLtk_TEST_D=OFF` to disable that target.

```sh
cmake -S tests -B build/tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
ldc2 -unittest -main DSLtk.d tests/test_graph_terms.d -of=dsltk-tests
./dsltk-tests
```
