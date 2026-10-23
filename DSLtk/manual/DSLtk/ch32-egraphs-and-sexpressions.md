# Chapter 32 — S-expressions, E-graphs and Equality Saturation

This chapter builds the workflow from rule text to equality-triggered
rewriting. Its APIs exist in `dsl::` in C++ and at module scope in D.

## 1. Construct and read the interchange language

An `SExpr` has `is_atom`, `text`, `items`, and `quoted` fields. Atoms carry text;
lists carry expressions. Prefer the `atom`/`list` factories, which establish
the appropriate representation and quoting. D expressions deeply copy their
mutable list storage; C++ expressions have ordinary vector value semantics.

```cpp
auto expression = dsl::SExpr::list({
    dsl::SExpr::atom("add"), dsl::SExpr::atom("a"), dsl::SExpr::atom("0")});
assert(expression.dump() == "(add a 0)");
auto parsed = dsl::parse_sexpr("; comment\n(add a 0)");
assert(parsed == expression);
```

```d
auto expression = SExpr.list([SExpr.atom("add"), SExpr.atom("a"), SExpr.atom("0")]);
assert(expression.dump() == "(add a 0)");
assert(parse_sexpr("; comment\n(add a 0)") == expression);
```

The notation is:

```ebnf
expression = atom | quoted_atom | "(" , { expression } , ")" ;
quoted_atom = '"' , { character | escape } , '"' ;
escape = "\\n" | "\\r" | "\\t" | '\\"' | "\\\\" ;
```

Whitespace separates unquoted atoms; parentheses delimit lists; a semicolon
starts a comment through the end of the line. Empty lists and quoted empty
atoms are valid S-expressions. Quotedness is retained. `dump()` quotes atoms
as necessary and escapes special characters. The reader bounds recursive
nesting at 512 levels and rejects malformed lists/escapes.

`parse_sexpr` requires exactly one expression. `parse_sexpressions` returns
all expressions in a source, including an empty sequence for empty input.

## 2. Understand tree conversion

`ast_to_sexpr` maps a leaf to `(tag value)` and an inner node to
`(tag child1 child2 ...)`. `sexpr_to_ast` maps a list with one atom argument
to a leaf, other headed lists to inner nodes, and a bare atom to tag `atom`.
An operator head must be an atom. That convention cannot distinguish an
inner node with one bare-atom child from a leaf payload; retain `SExpr` when
the interchange model itself is the desired syntax tree.

An e-graph accepts an `ASTNode` through `add(tree)` and produces one through
`extract_ast(id)` using these conventions.

## 3. Declare equality rules

```lisp
(rewrite add-zero (add ?x 0) ?x)
(rewrite mul-one (mul ?x 1) ?x)
(= (add ?left ?right) (add ?right ?left))
```

`sexpr_rule` reads one rule; `sexpr_rules` reads a sequence. Each
`ERewriteRule` stores `name`, `lhs`, and `rhs`. Shorthand `=` and `=>` rules
use the operator text as their name.

An unquoted atom starting with `?` and at least one following character is a
variable. Quoted `"?x"` is a literal. Variables may occupy complete term
positions, including the entire LHS, but operator heads are literal atoms.
Every RHS variable must occur in the LHS. Empty lists and list-valued heads
are invalid e-graph terms even though they are valid general S-expressions.

A rule's search orientation is LHS to RHS. Applying it records equality in
both directions. Supply a reverse-oriented rule when that reverse pattern
should independently trigger a search.

## 4. Add terms and establish equality

```cpp
dsl::EGraph graph;
auto a = graph.add("a");
auto b = graph.add("b");
auto fa = graph.add("(f a)");
auto fb = graph.add("(f b)");
assert(!graph.equivalent(fa, fb));
graph.unite(a, b);
assert(graph.equivalent(fa, fb));
```

Use `new EGraph` in D; the remaining calls have the same spelling.

An e-node records an operator, atom/quoted status, and child e-class IDs.
Hash-consing shares nodes with the same canonical children. Union-find stores
equivalence classes; `unite` marks the graph for congruence rebuilding.
`rebuild` discovers parent equivalences and removes duplicate canonical nodes.
Queries such as `equivalent`, `matches`, counts, and extraction rebuild first.
`find` returns an ID's current representative. IDs stay valid after merging.

`node_count()` counts retained unique e-nodes, whereas `class_count()` counts
equivalence classes. A class can retain several nodes/alternatives.

## 5. Match by equality

```cpp
graph.add("(pair a b)");
auto found = graph.matches(dsl::parse_sexpr("(pair ?x ?x)"));
assert(found.size() == 1); // a and b were merged above
```

```d
auto graph = new EGraph;
auto a = graph.add("a"), b = graph.add("b");
graph.unite(a, b);
graph.add("(pair a b)");
auto found = graph.matches(parse_sexpr("(pair ?x ?x)"));
assert(found.length == 1);
```

`EMatch.root` identifies the matched class; `bindings` maps variable names to
IDs. Matching explores alternatives within classes. Repeated variables compare
canonical class IDs, so equality is sufficient even when textual leaves differ.
`matches(pattern, limit)` bounds the number of returned records.

## 6. Saturate without discarding alternatives

`saturate(rules, limits, observer)` performs iterations of e-matching, RHS
instantiation, equality merging and rebuilding. It reaches saturation when an
iteration produces no graph revision. New RHS forms remain available to later
rules and to later iterations. The original forms remain represented too.

```cpp
dsl::EGraph graph;
auto root = graph.add("(mul (add a 0) 1)");
auto report = graph.saturate(dsl::sexpr_rules(R"(
    (rewrite add-zero (add ?x 0) ?x)
    (rewrite mul-one (mul ?x 1) ?x)
)"));
assert(report.saturated());
assert(graph.extract(root).dump() == "a");
```

`SaturationLimits` fields/defaults are:

| Field | Default | Bound |
|---|---:|---|
| `iterations` | 30 | Number of saturation iterations |
| `nodes` | 100000 | Retained graph node budget during insertion |
| `matches` | 100000 | Total successful rule applications |

`SaturationResult` exposes `iterations`, `applications`, `stop`, and
`saturated()`. `stop` is `Saturated`, `IterationLimit`, `NodeLimit`, or
`MatchLimit`. Partial results remain valid after a bounded stop; an interrupted
RHS insertion can leave newly inserted subterms without merging its unfinished
parent. The temporary node budget is restored when the run returns.

## 7. Trigger user rewriters

`when_equal` performs an explicit equality check and calls the supplied
rewriter with the graph and canonical representative:

```cpp
auto atom = graph.add("a");
graph.when_equal(root, atom, [](dsl::EGraph& g, dsl::EClassId id) {
    auto tree = g.extract_ast(id);
    // Apply domain-specific AST transformations or visitor operations here.
    assert(tree.tag() == "atom");
});
```

```d
auto atom = graph.add("a");
graph.when_equal(root, atom, (EGraph g, EClassId id) {
    auto tree = g.extract_ast(id);
    assert(tree.tag() == "atom");
});
```

The return value says whether the callback ran. Each call checks current
equality; this is a direct invocation API. For observing saturation itself,
its third argument receives `(ERewriteRule, EMatch)` after each completed
application. An application may report an already-established equality, and
a binding may be observed again in subsequent iterations. Keep observer
accounting separate from the graph-change criterion for saturation.

## 8. Extract a finite representative

`extract(id)` computes minimum node counts over the class alternatives, then
constructs an `SExpr`. Cyclic alternatives do not force a cyclic output when
a finite representative exists. For example, merging `a` with `(wrap a)`
still permits extracting `a`. There is no user-supplied extraction cost model
in the current API; the implemented metric is node count.

The integration examples linked from Chapter 30 combine rule preprocessing,
saturation, equality-triggered visitors and grammar recognition. Chapters 34
and 35 explain the visitor and preprocessing stages in detail.
