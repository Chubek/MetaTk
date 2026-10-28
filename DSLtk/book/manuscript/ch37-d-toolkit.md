# The D Toolkit: Mixins, CTFE, and Reflection

## A port that tests the abstraction

The D implementation is valuable both as a usable toolkit and as a comparison
that reveals which ideas are independent of C++. Composition, tree values,
parser commitment, and equality saturation do not intrinsically require CRTP,
class-type non-type template parameters, or `std::function`. D expresses
their roles with template mixins, native string arguments, alias callables,
operator templates, and compile-time function evaluation.

This chapter treats D as secondary without reducing it to a spelling table.
Some differences are merely syntax, such as a template argument written with
an exclamation mark. Others change behavior or cost. D's expression nodes
support indexed array evaluation; its PEG rules are reference objects; its
result type supports identical success and error payloads; and its repetition
parser checks for empty successful iterations. A serious porting guide must
identify these differences explicitly.

## Composition without a recursive base

`mixin DSL!(Features...)` injects declarations into the enclosing aggregate.
The built-in feature tags are empty structs, and the mixin uses static tests
of the selected tag list to inject convenience members. It also records the
list as `DSLFeatures`, rejects duplicate tags, and invokes a nested `Mixin`
template on custom feature tags when one exists.

This differs from the C++ arrangement where every built-in feature has a
nested mixin and `DSL` inherits all selected specializations. In D, the
custom-feature hook and the built-in static branches are distinct mechanisms.
The resulting host can still be queried with `HasFeature`, but the query
examines the recorded feature list rather than a C++ base-class relation.

## Compile-time values and callable binding

D permits strings directly as template arguments, so most APIs do not need
the optional `FixedString` compatibility wrapper. `leaf!"number"` and
`StaticRegex!"[0-9]+"` express their fixed vocabulary directly. CTFE evaluates
ordinary eligible functions when a compile-time value is required. An `enum`
manifest constant is a common way to request such evaluation.

Generic lambdas are naturally passed as aliases when their parameter types
remain templated. `pipe!((x) => x + x)()` uses `StaticCallable` to present an
invocable object without runtime callable storage. Typed functions, delegates,
and stateful callable structs can instead be passed as ordinary values.
Choose the form according to the callable's type and ownership rather than
assuming every lambda follows one universal syntax.

Predicate negation uses `~p` or `p.negated()` because D's logical negation
operator produces a Boolean rather than a new composable predicate. Parser
repetition uses `p.many()` or `many(p)` because unary star is not an
overloadable struct operator in the needed way. These are deliberate language
adaptations that preserve a role while changing its surface expression.

## Ownership differences that affect programs

The D AST is a struct with explicitly duplicated child storage on copying,
so modifying a copied tree's descendant does not modify the original tree.
That is a stronger contract than a default shallow copy of a dynamic array
would provide. The embedded tests exercise nested copies, which is important
because duplicating only the outermost array would not establish deep value
semantics for descendants.

PEG rules, definitions, and matchers are classes. Appending a new rule keeps
previous rule references pointing at the same allocated objects. This avoids
the C++ vector-element address invalidation problem, though it does not make
all mutable PEG state thread safe. Directed graphs and e-graphs are also
reference objects in D; assigning their references aliases state rather than
creating an independent speculative copy.

## Results, repetition, and indexed evaluation

The D result type stores a success discriminator and separate payload fields.
Tagged `OkWrap` and `ErrWrap` constructors disambiguate identical payload
types. `Result!(string,string)` can therefore be queried and extracted
correctly, which makes the D task pipeline a working realization of the
same high-level design that is blocked by the C++ snapshot's duplicate-type
variant queries.

The D repetition parser checks whether a successful child advanced the cursor.
If not, it reports a committed nonempty-repetition error instead of looping
forever. This does not make a zero-width repeated grammar sensible; it makes
the failure observable. Portable grammars should still establish progress
as an invariant rather than depend on a language-specific diagnostic.

Indexed expression nodes expose a length and element evaluation operation.
A combined array expression can compute each output element from its
operands without materializing every intermediate array. Length mismatch is
checked. This mechanism is absent from the C++ `BinExpr::eval`, which evaluates
whole operand values and invokes their operators. Similar expression syntax
therefore carries different fusion capabilities in the two files.

## Native enum reflection

D's reflection implementation enumerates declared members through language
traits. It removes duplicate values according to its policy, orders reflected
values, and supports user-defined attributes for flags intent and display
names. There is no default minus-128-to-127 discovery interval analogous to
the embedded C++ implementation. Large sparse enumerator values can be
discovered without scanning every intervening integer.

That convenience does not replace application compatibility policy. Renaming
a member still changes an uncustomized external name; changing an explicit
numeric value still changes a numeric wire representation. Native reflection
improves how the table is obtained, not what guarantees a persisted format
should make. The same boundary design principles from Chapter 7 apply.

## A verified importing program

`projects/d-overview.d` imports the module rather than relying only on tests
inside it. It exercises a mixed-feature host, an alias-bound pipe, deep AST
copying, a same-type result, sparse enum reflection, and equality saturation.
The book's `make d-check` command builds and runs it with LDC. Its output is:

```text
10 (add (number 1) (number 2)) a
```

The complete importing program is included from its maintained source:

```{=latex}
\lstinputlisting[style=dsld,caption={D companion: importing the module and checking language-specific contracts.},label={lst:d-companion}]{projects/d-overview.d}
```

The embedded module tests provide broader coverage, while an external import
checks visibility and real consumer usage. Keep those forms of evidence
separate. A module can compile its own private helper tests yet expose a
problem to an importing client, and a tiny client can pass without exercising
every embedded algorithm. Together they provide a more useful verification
picture than either one alone.

The reference guide that follows maps the established feature families to D.
The later shared subsystems--S-expressions, e-graphs, codecs, I/O, visitors,
static byte regexes, preprocessing, CFGs, graphs, terms, and reduction--also
exist in the module. Their earlier chapters explain the algorithms primarily
through C++; this chapter supplies the ownership and language mechanisms
needed to carry those explanations over accurately.


`DSLtk.d` is a self-contained D module using Phobos. This chapter maps the
established C++ toolkit to D and introduces the language-specific API. The
shared facilities introduced in Chapter 3 have focused tutorials in Chapters
32–36.

## Build and import

Keep `DSLtk.d` on the import path and supply the module to the compiler:

```sh
# From the DSLtk directory, with the user's app.d alongside the module:
ldc2 app.d DSLtk.d -of=app
dmd app.d DSLtk.d -of=app
gdc app.d DSLtk.d -o app
```

For an application in another directory, add the module's directory to `-I`.
The CMake installation includes the D source module. Its C++ `INTERFACE`
target exports C++ usage requirements; D compilation is driven by the D
compiler invocation above.

```d
import DSLtk;
```

## Compose features with a mixin

```d
struct MathDSL
{
    mixin DSL!(Pipeline, Operators, AST, ExprTemplates);
    double val;
    double expr_value() const { return val; }
}
```

The mixin adds selected methods directly to the enclosing aggregate. Features
must be unique. `DSLFeatures` records the selected types; `HasFeature!(T, F)`
queries that list. The convenience predicates are `Pipeable!T`, `ExprLike!T`,
`HasLiterals!T`, and `Rewritable!T`.

| Feature | Injected convenience operations |
|---|---|
| `Pipeline` | `wrap(value)` |
| `Operators` | `make_pred(fn)` |
| `AST` | `make_leaf!tag(value)`, `make_node!tag(children...)` |
| `ExprTemplates` | Arithmetic operator templates; requires `expr_value()` |
| `Rewrite` | `rewrite(tree)` using the aggregate's `rules` |
| `CustomLiterals` | `parse_literal(text)` using its static `literals` |
| `Memoization` | `clear_cache()` for the aggregate's `memo_cache` |

Other built-in tags record feature presence while their facilities are used
through module-level functions/types. Their list includes `PatternMatch`,
`LazyFeature`, `Monadic`, `ResultFeature`, `CombinatorParser`, `TaskPipeline`,
`EnumReflection`, and the later shared facility tags catalogued in Chapter 3.

A custom feature can provide a nested mixin:

```d
struct DomainFeature
{
    mixin template Mixin()
    {
        int domain_value() const { return 17; }
    }
}
struct DomainDSL { mixin DSL!(Pipeline, DomainFeature); }
static assert(HasFeature!(DomainDSL, DomainFeature));
```

## Callable values and alias callables

Typed functions, delegates and functors can be passed as ordinary values:

```d
auto result = 5 | pipe((int x) => x * 2) | pipe((int x) => x + 1);
assert(result == 11);
```

Generic lambdas can instead be passed as template aliases:

```d
auto doubleIt = pipe!((x) => x + x)();
assert((4 | doubleIt) == 8);
auto positive = predicate!((x) => x > 0)();
assert(positive(2) && positive(2.5));
```

`StaticCallable!fn` binds an alias callable to a concrete functor. Alias
factories include `pipe!fn()`, `predicate!fn()`, `when!(key, fn)()`,
`otherwise!fn()`, `on!(tag, fn)()`, and `enum_switch!fn(value)`.

## Pipelines, predicates and dispatch

Predicates compose through `&` and `|` with short-circuit evaluation. Use `~`
or `negated()` for predicate negation; D's `!` produces a boolean.

```d
auto positive = predicate((int x) => x > 0);
auto even = predicate((int x) => x % 2 == 0);
auto valid = positive & even;
assert(valid(4) && !valid(3));
assert((~positive)(-2));
```

Dispatch handlers receive the key, followed by any additional arguments:

```d
auto table = match(
    when!1((int key) => "one"),
    otherwise((int key) => "other"));
assert(table(1) == "one");

auto classify = match(
    when!(pattern!"[0-9]+")((string key) => "number"),
    otherwise((string key) => "other"));
assert(classify("123") == "number");
```

Handlers must have a common non-void result type. An unmatched key without an
`otherwise` clause raises an exception. `pattern!S` is the small whole-input
matcher for literals, dot, classes/ranges, anchors and `*`/`+`; the broader
byte-NFA API is `StaticRegex!S` in book Chapter 32.

## Value-type ASTs and bottom-up rewriting

String tags are native template parameters:

```d
auto tree = node!"add"(leaf!"num"(1), leaf!"num"(2));
assert(tree.dump() == "(add (num 1) (num 2))");
auto copy = tree;
copy.child(0) = leaf!"num"(10);
assert(tree.child(0).value() == "1");
```

`ASTNode` deeply copies mutable child storage. Its accessors are `tag`,
`is_leaf`, `value`, `children`, `child`, `size`, `dump`, and `toString`.

```d
import std.conv : to;
auto rules = rewrite_set(rule!"fold-add"(
    (ASTNode n) => n.tag() == "add" && n.size() == 2 &&
                  n.child(0).is_leaf() && n.child(1).is_leaf(),
    (ASTNode n) => leaf!"num"(
        to!int(n.child(0).value()) + to!int(n.child(1).value()))));
assert(rules.apply(tree).dump() == "(num 3)");
```

Rules run bottom-up and in registration order. `max_iterations` defaults to
100; passes stop when the tree no longer changes. Equality saturation in
Chapter 29 retains multiple equivalent expressions rather than replacing the
current tree on each rule application.

## Lazy expressions, literals and caches

`terminal(value)` lifts a scalar or array into an expression tree. Array
expressions evaluate element by element into one result array; participating
arrays must have matching lengths. Scalars can participate in array operations.

```d
auto a = terminal([1, 2, 3]);
auto b = terminal([4, 5, 6]);
auto c = terminal([7, 8, 9]);
assert((a + b * c).eval() == [29, 42, 57]);
assert((2 * a).eval() == [2, 4, 6]);
```

Terminal arrays use D's slice storage semantics. Copying a terminal does not
deep-copy the referenced array. `ASTNode` explicitly supplies deep-copy
behavior for its child tree.

```d
auto units = literal_set(
    lit!"_km"((real n) => n * 1000), lit!"_m"((real n) => n));
assert(units.parse("3.5_km") == 3500);
enum distance = literal_set(lit!"_km"((real n) => n * 1000)).parse("3.5_km");
static assert(distance == 3500);
```

`memoize!(Key, Value)(fn)` caches results by associative-array key and provides
`clear_cache`. `make_lazy(fn)` infers the producer/result types; `Lazy!T(fn)`
accepts a delegate/function producer. `get()` and `force()` return the cached
value by reference. `is_computed()` reports whether production has occurred.

## Optional and result flows

`Maybe!T` supplies `map`, `flat_map`, `filter`, `or_else`, `has_value`,
`value`, and `as_optional`. The module-level optional helpers accept Phobos
`Nullable!T` values:

```d
import std.typecons : Nullable;
assert(Maybe!int(5).map((int n) => n + 1).or_else(0) == 6);
assert(map(Nullable!int(3), (int n) => n * 2).get == 6);
```

`Result!(T, E)` explicitly tags success/error branches, including when `T` and
`E` are identical. Use `from_ok`, `from_err`, or `Ok`/`Err` wrappers. Operations
are `is_ok`, `is_err`, `unwrap`, `unwrap_err`, `unwrap_or`, `map`, `and_then`,
and `map_err`. Incorrect unwrap operations raise an exception.

```d
alias R = Result!(string, string);
auto ok = R.from_ok("value");
auto err = R.from_err("error");
assert(ok.map((string s) => s.length).unwrap() == 5);
assert(err.map((string s) => s.length).unwrap_err() == "error");
```

## Parsers, semantic actions and PEG

`parser(fn)` wraps a callable taking `ref ParsecInput` and returning
`ExpectedResult!T`. Composition preserves concrete parser/callable types:

| Operation | D syntax |
|---|---|
| Sequence | `a & b` returns a `Tuple` |
| Choice | `a | b` with equal output types |
| Zero-or-more | `p.many()` / `many(p)` |
| Optional | `p.optional()` / `optional(p)` |
| Backtracking | `try_parse(p)` |
| Expected-label replacement | `labeled(p, label)` |
| Whole-source execution | `run_parser(p, source)` |

```d
auto pair = ch('a') & ch('b');
auto alternate = try_parse(pair) | (ch('a') & ch('c'));
assert(!run_parser(alternate, "ac").value.isNull);
auto digits = satisfy((char c) => c >= '0' && c <= '9', "digit").many();
assert(run_parser(digits, "123").value.get == ['1', '2', '3']);
```

Inspect `value.isNull` and `error` on a `ParseOutcome`. Failures distinguish
`Soft` and `Committed`; `try_parse` restores the cursor and softens a failure.
Repetition rejects successful parsers that consume no input. `parse_with_action`
passes a `ParserStage` span to a user callback; `production!id(fn)` stores a
compile-time production identifier with its handler.

PEG combinators are `peg_seq`, `peg_choice`, `peg_opt`, `peg_many`, `peg_many1`,
`peg_and`, `peg_not`, `peg_rule`, and `peg_channel`. `create_peg_definition()`
and `derive_peg_definition(parent)` return reference-type definitions.
`add_rule!pattern(action)` returns a stable rule reference. Channels, semantic
actions and fluent matcher arms follow Chapters 27–28, using D callbacks such
as `(ref PEGMatch m) { ... }`. PEG patterns retain their documented greedy
prefix semantics.

## Tasks and shared state

`Task(name, callback)` takes a `ref TaskState` callback returning
`Result!(string, string)`. `task1 | task2` creates a `TaskChain`; `run` records
each result by task name. Policies are `StopOnError` and `ContinueOnError`.
`suspend(state)` stops subsequent work; `upon` runs a task when a supplied
event callable returns true. Stateful caches, lazy values and PEG objects do
not provide synchronization.

## Native enum reflection

```d
enum Color { Red = -1000, Green = 0, AliasGreen = 0, Blue = 1000 }
static assert(enum_count!Color() == 3);
static assert(enum_name!(Color.Red)() == "Red");
static assert(enum_values!Color() == [Color.Red, Color.Green, Color.Blue]);
auto blue = enum_cast!Color("blue", case_insensitive);
assert(blue.get == Color.Blue);
auto name = enum_switch!((c) => enum_name!(typeof(c).value)())(Color.Blue);
assert(name == "Blue");
```

Declared members are reflected without numeric search-range limits. Values
are sorted numerically and duplicate values use the first declared name.
Checked casts/indexing and adjacency return `Nullable`; arrays of values,
names and entries are returned as fixed-size values. `enum_reflected!E(integer)`
checks reflected membership in D. `enum_switch` has an optional fallback;
its no-fallback form raises an exception for an unknown value.

```d
@(EnumOptions(true)) enum Access { Read = 1, Write = 2, Execute = 4, All = 7 }
assert(enum_flags_name(Access.Read | Access.Write) == "Read|Write");
@(EnumOptions(false, 2)) @(EnumNames(["c_Red": "rouge"]))
enum Named { c_Red, c_Blue }
static assert(enum_name(Named.c_Red) == "rouge");
static assert(enum_name(Named.c_Blue) == "Blue");
```

`EnumOptions` sets flags mode and prefix trimming; `EnumNames` maps declaration
names to display names. Flags reflection selects single-bit members. Helpers
include `enum_flags_cast`, `enum_flags_contains`, `enum_flags_test`, and
`enum_flags_test_any`. `EnumArray!(E, V)`, `EnumBitset!E`, and `EnumSet!E` supply
enum-indexed containers. D's native enum formatting/bitwise operators supply
the host-language integrations. Chapter 7 documents C++'s upstream-specific
configuration, namespace aliases and stream/formatter APIs.

## Verify the D module

```sh
ldc2 -unittest -main DSLtk.d -of=dsltk-test
./dsltk-test
dmd -unittest -main DSLtk.d -of=dsltk-test-dmd
./dsltk-test-dmd
gdc -funittest -fmain DSLtk.d -o dsltk-test-gdc
./dsltk-test-gdc
```

The tests cover established features and the new shared facilities. Build the
integration example with `ldc2 -I. DSLtk.d examples/26-equality-saturation.d
-of=eqsat-example`, then run `./eqsat-example`.

## Implementation map and review

The following diagram summarizes the chapter's central dependency or state
transition. Arrows denote logical dependence, not elapsed time or measured
performance. The contracts below separate what the library provides from the
conditions that the application must establish.

```{=latex}
\begin{figure}[htbp]
\centering
\begin{tikzpicture}
\node[dslsmall] (n0) {D mixin};
\node[dslsmall, right=5mm of n0] (n1) {Native strings};
\draw[dsledge] (n0) -- (n1);
\node[dslsmall, right=5mm of n1] (n2) {CTFE};
\draw[dsledge] (n1) -- (n2);
\node[dslsmall, right=5mm of n2] (n3) {Runtime object};
\draw[dsledge] (n2) -- (n3);
\end{tikzpicture}
\caption{The D port uses native language mechanisms for the same toolkit roles.}
\label{fig:atlas-37}
\end{figure}
\begin{table}[htbp]
\centering\small
\begin{tabular}{L{0.28\linewidth}L{0.61\linewidth}}
\toprule Contract & Interpretation \\
\midrule
Composition & DSL injects declarations into the aggregate. \\
Reflection & Declared enum members replace C++ numeric scanning. \\
Results & A Boolean discriminant permits identical payload types. \\
\bottomrule
\end{tabular}
\caption{Implementation contracts for this chapter.}
\label{tab:atlas-37}
\end{table}
\begin{pseudocode}[Operational summary]
\textbf{1.} Import the module and choose feature tags.\\
\textbf{2.} Inject DSL into the aggregate.\\
\textbf{3.} Bind generic callables as aliases when needed.\\
\textbf{4.} Compute eligible static data through CTFE.\\
\textbf{5.} Check behavior rather than mechanically translating syntax.
\end{pseudocode}
```

### Review questions

1. Why does predicate negation use a different operator in D?

2. Which C++ lifetime problem disappears when PEG rules are reference objects?
