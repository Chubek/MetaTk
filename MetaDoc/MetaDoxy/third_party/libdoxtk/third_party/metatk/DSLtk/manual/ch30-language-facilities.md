# Chapter 30 — Equality Saturation and Language Facilities (C++ and D)

These facilities are in `DSLtk.hpp` (`dsl::`) and `DSLtk.d` (`import DSLtk`).
They use only the standard library unless the optional Lua bridge is enabled.
Their feature tags are `SExpressions`, `EqualitySaturation`, `Serialization`,
`AbstractIO`, `Visitors`, `StaticRegularExpressions`, `Preprocessing`, and
`ContextFreeGrammar`. They can also be used directly, without a DSL base/mixin.

This chapter is the shared overview. Continue with:

* [Chapter 31: the D port and established feature families](ch31-d-toolkit.md);
* [Chapter 32: S-expressions, e-matching and saturation](ch32-egraphs-and-sexpressions.md);
* [Chapter 33: codecs, abstract I/O, static regexes and binary streams](ch33-codecs-io-and-binary-streams.md);
* [Chapter 34: AST-schema-derived visitors](ch34-ast-derived-visitors.md);
* [Chapter 35: preprocessing and optional native Lua](ch35-preprocessor-and-lua.md);
* [Chapter 36: explicit CFGs and Earley parsing](ch36-context-free-grammars.md).

The [development progression record](progression.md) ties the implementation
milestones, packaging, examples and verification together.

## S-expressions

`SExpr` is a value containing an atom or a list. `SExpr.atom` / `SExpr::atom`
and `list` construct expressions. `parse_sexpr` reads one expression;
`parse_sexpressions` reads multiple. `dump()` produces a readable representation.
Semicolon comments, nested lists, quoted atoms, and `\n`, `\r`, `\t`, `\"`,
`\\` escapes are supported. Invalid input raises an exception.

`ast_to_sexpr` and `sexpr_to_ast` bridge the existing runtime AST. An AST leaf is
represented by `(tag value)`; an inner node by `(tag children...)`. Consequently
an S-expression with one atom argument converts to an AST leaf, and bare atoms
convert to `leaf<"atom">` / `leaf!"atom"`. Use `SExpr` itself when that distinction
matters. Lists used as e-graph terms require an atom operator head.

## E-graphs and equality saturation

An `EGraph` stores hash-consed e-nodes in union-find e-classes. `unite` establishes
equality; `rebuild` restores congruence. For example, merging `a` and `b` also
makes `(f a)` and `(f b)` equivalent. Public IDs remain usable after merges;
`find` returns their current representative.

Rules are S-expressions:

```lisp
(rewrite add-zero (add ?x 0) ?x)
(rewrite mul-one (mul ?x 1) ?x)
(= (add ?a ?b) (add ?b ?a))
```

`?name` denotes a pattern variable. A repeated variable must match the same
e-class, not necessarily the same textual expression. Quoted `"?name"` is data.
RHS variables must be bound on the LHS. `(=> lhs rhs)` is accepted too.
Rules match their LHS and introduce their RHS; equality itself is symmetric,
but matching in the reverse orientation requires another rule.

C++:

```cpp
dsl::EGraph graph;
auto root = graph.add("(mul (add a 0) 1)");
auto rules = dsl::sexpr_rules(R"(
    (rewrite add-zero (add ?x 0) ?x)
    (rewrite mul-one (mul ?x 1) ?x)
)");
auto report = graph.saturate(rules);
assert(report.saturated());
assert(graph.extract(root).dump() == "a");
```

D:

```d
auto graph = new EGraph;
auto root = graph.add("(mul (add a 0) 1)");
auto rules = sexpr_rules(`
    (rewrite add-zero (add ?x 0) ?x)
    (rewrite mul-one (mul ?x 1) ?x)
`);
auto report = graph.saturate(rules);
assert(report.saturated());
assert(graph.extract(root).dump() == "a");
```

`matches(pattern)` returns `EMatch` records (`root`, `bindings`). The optional
third argument to `saturate` observes each successful rule application. It may
run more than once for a binding across iterations. `when_equal(a, b, callback)`
calls a user rewriter with the graph and representative only when the IDs are
equivalent. Thus a user can respond to equality independently of saturation.

`SaturationLimits` bounds iterations, retained nodes, and total applications.
The result reports `Saturated`, `IterationLimit`, `NodeLimit`, or `MatchLimit`.
A bounded run retains valid partial results. `extract` selects a minimum-node
finite expression even when the graph contains cycles. `extract_ast` converts
that representative back to the runtime AST.

## Abstract serialization and deserialization

The library defines protocols and forwarding adapters, with no prescribed
wire syntax. A user backend supplies:

| Operation | C++ | D |
|---|---|---|
| Serialize | `backend.serialize(value, output)` | same |
| Deserialize | `backend.template deserialize<T>(input)` | `backend.deserialize!T(input)` |

`Serializer<Backend>` / `Serializer!Backend` and `Deserializer` borrow that
backend and preserve its return type and error handling. Inputs and outputs
can be any user type; they need not be binary streams. `SerializerBackend`
and `DeserializerBackend` expose protocol checks.

## Abstract byte I/O

`IO<Backend>` / `IO!Backend` borrows a backend implementing:

* `read(mutable byte buffer) -> transferred count`;
* `write(const byte buffer) -> transferred count`.

C++ buffers are `std::span<std::byte>` / `std::span<const std::byte>`; D buffers
are `ubyte[]` / `const(ubyte)[]`. Zero read means EOF. Short transfers are valid.
`read_exact` and `write_all` loop over short transfers; premature EOF, invalid
counts, or a non-progressing write raise an exception. `seek` and `tell` are
available when the backend supplies them. Backend lifetime is user-managed.

## Static regexes and binary streams

`StaticRegex<"pattern">` / `StaticRegex!"pattern"` compiles a Thompson NFA at
compile time. Matching treats input as bytes, including NUL and bytes above
`0x7f`. Supported syntax:

* literals and escaped literals;
* `.` (any byte), classes/ranges, negated classes;
* grouping `(...)`, alternation `|`, repetition `*`, `+`, `?`;
* beginning/end assertions `^`, `$`;
* `\xHH`, `\0`, `\n`, `\r`, `\t`.

There are no capture groups, backreferences, shorthand classes such as `\d`,
or counted repetitions. `matches` requires a whole-input match. `prefix`
returns the longest matching end offset from a position; `search` returns the
earliest matching span, choosing the longest match at that position.

`BinaryStream` owns a byte buffer and cursor. `read(count)` returns a span/slice;
`next<Regex>()` / `next!Regex()` consumes a regex token, leaving the cursor
unchanged on failure. Empty tokens raise an exception. `write` appends bytes.
`read_integer<T>` / `read_integer!T` and `write_integer` use explicit
`ByteOrder.Little/Big` (`::` in C++). `load(io, limit)` reads until EOF within
a size bound; `save(io)` writes through the abstract I/O protocol.

## Derive a visitor from an AST

Handlers are registered with `on<"tag">(fn)` / `on!"tag"(fn)`. A handler may take
the node, or a compile-time `ASTTag` followed by the node. `derive_visitor(schema,
handlers...)` derives the accepted tag set from a runtime AST and constructs a
template-specialized dispatch table. Missing handlers are rejected immediately;
visiting a tag outside the schema is rejected. `visit` visits one node, and
`walk` traverses in `VisitOrder.Preorder` or `Postorder`.

For compile-time schema validation, C++ uses a type-level AST:

```cpp
auto schema = dsl::ast_schema<"add">(dsl::ast_schema<"num">());
auto visitor = dsl::compile_visitor(schema,
    dsl::on<"add">([](const dsl::ASTNode&) {}),
    dsl::on<"num">([](const dsl::ASTNode&) {}));
```

D can use the actual runtime AST type at CTFE:

```d
enum schema = node!"add"(leaf!"num"(0));
auto visitor = compile_visitor!schema(
    on!"add"((ref ASTNode n) {}),
    on!"num"((ref ASTNode n) {}));
```

This compiles handler dispatch and validates a static schema; it is not a
runtime JIT compiler. Visitor callbacks supply the semantic operations.

## Built-in preprocessor and optional Lua

`Preprocessor` supports object/function macros, balanced arguments, recursive
macro suppression, backslash-newline continuation, include directives, and
conditional blocks. Macro expansion respects strings, character literals,
and C-style comments. Conditional directives are `#if`, `#ifdef`, `#ifndef`,
`#elif`, `#else`, `#endif`. Conditions support integer constants, `defined`,
`!`, comparisons, `&&`, and `||`. Other operations are `#define`, `#undef`,
`#include`, `#error`, `#lua code`, and `#script language code`.

This is a DSL preprocessor with the documented subset, not a complete C/C++
preprocessor: token-pasting, stringification, variadic macros, and arithmetic
in `#if` are not implemented. `include` is a user callback returning source
for a path; `script` returns source for a language and code string. Generated
script output is macro-expanded. Expansion/include depths are bounded.

For actual Lua embedding, C++ defines `DSLTK_ENABLE_LUA`, supplies the Lua
include/link flags, and assigns `pp.script = dsl::lua_script_hook(state)`.
D builds with `-d-version=DSLtkLua` (LDC), `-version=DSLtkLua` (DMD), or
`-fversion=DSLtkLua` (GDC), links Lua 5.3/5.4, and assigns
`pp.script = lua_script_hook(state)`. The user owns/configures the Lua state.
Each directive returns a string. The bridge restores the Lua stack after
success or failure. For example: `#lua return '(= a b)'`.

## Explicit context-free grammar

`CFG` has named nonterminals, token terminals, and productions. Its Earley
parser handles left recursion, epsilon productions and unordered alternatives.
It returns an AST for the first discovered derivation of an ambiguous grammar,
or a farthest token position and expected terminals. The user's lexer supplies
the input token sequence.

Grammar S-expression notation:

```lisp
(grammar
    (start Expr)
    (rule Expr (Expr "+" Term) (Term))
    (rule Term ("number")))
```

Unquoted RHS symbols are nonterminals; quoted symbols are literal tokens.
Each RHS list is an alternative; `()` is epsilon. `sexpr_grammar` loads this
notation and validates references. Alternatively, construct `CFGSymbol` and
`CFGRule` values, or use `CFGTerminal`, `CFGNonterminal`, `CFGProduction` and
`make_cfg` templates. The resulting AST tags are production names; terminal
leaves use the tag `token`.

## Enum reflection

C++ retains the complete embedded `magic_enum` public API described in Chapter
29. D provides native reflection through `enum_name`, `enum_values`,
`enum_names`, `enum_entries`, `enum_count`, checked casts/indexing, adjacency,
compile-time dispatch/fusion, flags and enum-indexed containers. It has no
numeric search-range restriction. `EnumOptions` and `EnumNames` UDAs customize
flags, prefixes and display names. D's native formatting and bitwise operators
replace the C++ stream/operator integrations.

## Verification

From the DSLtk directory:

```sh
cmake -S tests -B build/dsltk-tests -DDSLtk_TEST_LUA=ON
cmake --build build/dsltk-tests
ctest --test-dir build/dsltk-tests --output-on-failure
ldc2 -unittest -main DSLtk.d -of=dsltk-test
./dsltk-test
```

The C++ test suite includes `test_language_facilities.cpp` and an optional
native Lua bridge test. D's tests are embedded in its single module.
