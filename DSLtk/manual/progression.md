# DSLtk Development Progression

This record covers the progression from the C++ toolkit to its D counterpart,
then to the shared language-construction facilities and their documentation.
It describes implemented work. API details and tutorials are linked below.

## 1. Starting point: the C++ toolkit

`DSLtk.hpp` supplies feature composition through `DSL<Derived, Features...>`
and CRTP mixins. Its established facilities are pipelines, predicates, pattern
dispatch, value-type ASTs, bottom-up rewrite passes, expression templates,
custom literals, memoization, lazy values, monadic optionals, results, parser
combinators, task pipelines, PEG definitions/matching, and enum reflection.

The complete public headers of `magic_enum` v0.9.8 are embedded in the C++
header. Reflection, flags, dispatch, fusion, containers, streams, and optional
formatting are documented in [Chapter 29](ch29-enum-reflection.md).

## 2. Translating the toolkit to D

The D implementation was created as one module, `DSLtk.d`, using Phobos.
Its design follows the behavior of the C++ toolkit while using D mechanisms:

| C++ foundation | D implementation |
|---|---|
| CRTP/multiple-inheritance mixins | `mixin DSL!(Features...)` in the user's aggregate |
| `FixedString` non-type template arguments | Native string template arguments |
| Recursive callable/template types | Concrete callable structs, variadic tuples and operator templates |
| `constexpr` computations | Compile-time function evaluation (CTFE) |
| Generic lambdas passed as template parameters | Alias callables and `StaticCallable` |
| Optional values | `Nullable` and `Maybe` |
| Embedded `magic_enum` implementation | Native enum-member reflection and UDA customization |

All established feature families received D implementations. The port includes
deep copying of AST child storage, fused array expression evaluation, parser
backtracking and diagnostics, PEG rule identity through reference types, and
success/error tagging that permits identical `Result` payload types.

Two syntax adaptations are visible to callers: predicate negation is `~pred`
or `pred.negated()`, and parser repetition is `p.many()` or `many(p)`.
Generic lambdas can be bound as aliases, for example `pipe!((x) => x + x)()`.
Custom feature tags may provide a nested `Mixin` template.

The initial port was verified with embedded tests under LDC, DMD and GDC,
and with a separate importing module. The full D guide is
[Chapter 31](ch31-d-toolkit.md).

During portability checks, eagerly inferred recursive `Parser` convenience
methods caused compiler memory exhaustion. Making `many()()` and
`optional()()` function templates deferred their instantiation and resolved
that issue. Subsequent compiler checks completed with a 1 GiB virtual-memory
limit. This is an implementation detail; callers retain `p.many()` and
`p.optional()` syntax.

## 3. Adding a shared S-expression substrate

Both files gained `SExpr`, atom/list constructors, readers, readable dumping,
quoted atoms, escapes, semicolon comments, and AST conversion. The same
representation supplies the syntax for equality rules and explicit grammars.

`parse_sexpr` reads one expression; `parse_sexpressions` reads a sequence.
`ast_to_sexpr` and `sexpr_to_ast` connect the new notation to `ASTNode`.
Quotedness is retained, including the distinction between a variable `?x` and
the literal quoted atom `"?x"` in a rule.

## 4. E-graphs and equality-triggered rewriting

Both implementations gained hash-consed e-nodes, union-find e-classes,
congruence rebuilding, e-matching, and equality saturation. Rules are written as
`(rewrite name lhs rhs)`, `(= lhs rhs)`, or `(=> lhs rhs)`.

Matching binds variables to e-classes. Applying a rule introduces its RHS and
merges that result with the matched class. Equivalent alternatives are retained
for subsequent matching and extraction. Repeated variables enforce e-class
equality. `when_equal` invokes a user rewriter after an explicit equality check;
the saturation observer reports successful applications.

Runs are bounded by iteration, node and application limits. Reports distinguish
saturation from each limit. Minimum-node extraction supports finite results
from graphs containing cycles. See [Chapter 32](ch32-egraphs-and-sexpressions.md).

## 5. User-defined data boundaries

`Serializer` and `Deserializer` became template adapters over user codecs.
They forward operations and their return types; the user's backend supplies
the syntax and semantic operations.

`IO` became a byte-I/O adapter over user backends. It checks transferred counts,
supports partial transfers, and provides exact-read/write-all helpers.
Seeking and position queries are available when the backend supplies them.
These adapters borrow their backends rather than choosing an ownership model
or a filesystem/network implementation for the user.

## 6. Static byte regexes and binary streams

A Thompson NFA compiler was added to both files. `StaticRegex` compiles a
template-supplied expression and operates on all 256 byte values. Supported
syntax includes grouping, alternation, character classes/ranges, repetition,
anchors and byte escapes. Matching has whole-input, prefix and search APIs.

`BinaryStream` supplies a buffer/cursor, regex-token reads, explicitly ordered
unsigned integer operations, and load/save integration with abstract I/O.
Empty token matches are rejected, and failed token reads leave the cursor
unchanged. Data-boundary and regex APIs are in
[Chapter 33](ch33-codecs-io-and-binary-streams.md).

## 7. Deriving visitors from AST schemas

Visitors are built from an AST schema and typed `on` handlers. Their dispatch
is generated through templates. Runtime schemas are checked when the visitor
is created; static schemas are checked during compilation.

C++ uses the structural `ASTSchema`/`ast_schema` type-level tree for static
schemas. D can use an actual `ASTNode` constructed at CTFE. Visitors support
single-node dispatch and preorder/postorder walks. A handler may receive an
`ASTTag` compile-time constant as well as the runtime node. See
[Chapter 34](ch34-ast-derived-visitors.md).

## 8. Preprocessing and optional Lua embedding

Both implementations gained a preprocessor supporting object/function macros,
balanced arguments, continuation lines, recursive macro suppression,
conditional directives, includes, and scripting directives. Include resolution
and script execution are user callbacks.

Native Lua bridges are optional: `DSLTK_ENABLE_LUA` selects the C++ bridge and
the D version identifier `DSLtkLua` selects the D bridge. The user owns the Lua
state and provides the library linkage. The bridge runs a directive, obtains
its generated source, and restores the Lua stack on success or failure.

The implemented directive and expression subsets, state behavior, and native
Lua build instructions are in [Chapter 35](ch35-preprocessor-and-lua.md).

## 9. Explicit context-free grammars

`CFG` supplies terminal/nonterminal symbols and named productions, with both
value-based and template-based construction. `sexpr_grammar` loads a compact
S-expression grammar notation.

The parser uses Earley chart processing, supports left recursion and epsilon
productions, and constructs an AST for the first discovered derivation. A
failure reports its farthest token position and expected terminal tokens.
The existing PEG facilities continue to expose ordered-choice parsing. The
CFG tutorial is [Chapter 36](ch36-context-free-grammars.md).

## 10. Packaging, examples and verification

The CMake install rule includes `DSLtk.hpp` and `DSLtk.d`. The C++ example
target `dsltk_equality_saturation_example` accompanies the enum example.
Source examples in both languages combine preprocessing, equality saturation,
equality-triggered visitors, grammar parsing and static regex matching:

* [`26-equality-saturation.cpp`](../examples/26-equality-saturation.cpp)
* [`26-equality-saturation.d`](../examples/26-equality-saturation.d)

`tests/test_language_facilities.cpp` covers the new C++ facilities.
`tests/test_lua_preprocessor.cpp` covers the optional native bridge.
Equivalent D coverage is embedded in `DSLtk.d`.

At the completion of the implementation, all seven configured C++ tests passed,
including the enum/formatting tests and the optional Lua test. The new C++
facility tests also passed with Clang. D tests passed under LDC, DMD and GDC;
the native Lua bridge was additionally exercised under LDC. Both integration
examples compiled and ran. These are recorded implementation checks; current
build instructions are in [Chapter 2](ch02-getting-started.md) and the focused
chapters.

## 11. Manual progression

Chapter 30 provides a shared overview. Chapters 31–36 expand that overview into
a D guide and focused tutorials for every added facility. The introduction,
getting-started guide, enum chapter, and table of contents now connect the
original C++ material to the two-language toolkit.
