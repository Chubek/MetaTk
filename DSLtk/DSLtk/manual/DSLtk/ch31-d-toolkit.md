# Chapter 31 — The D Toolkit: Templates, Mixins and CTFE

`DSLtk.d` is a self-contained D module using Phobos. This chapter maps the
established C++ toolkit to D and introduces the language-specific API. The
shared facilities introduced in Chapter 30 have focused tutorials in Chapters
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
`EnumReflection`, and the eight shared facility tags from Chapter 30.

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
byte-NFA API is `StaticRegex!S` in Chapter 33.

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
Chapter 32 retains multiple equivalent expressions rather than replacing the
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
the host-language integrations. Chapter 29 documents C++'s upstream-specific
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
