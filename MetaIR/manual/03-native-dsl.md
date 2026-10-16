# 3. The C++-native DSL and DSLtk

## What is native about this DSL?

The program is described with ordinary C++ expressions and method calls. Its
expression values are DSLtk `ASTNode`s. Script and Toolchain use DSLtk's CRTP
`DSL` base and its pipeline feature; expression construction calls DSLtk
`leaf` and `node`. There is no separate home-grown expression-template engine.

The word *native* here means C++-embedded syntax. A script can be interpreted,
serialized, optimized, or compiled to machine code. Constructing a script does
not by itself execute its expressions.

## Values, registers, and expressions

```cpp
using namespace metachain::native;
auto x = reg(0);
auto y = reg(1);
Expr arithmetic = x * Expr(7) + y;
```

`reg(n)` accepts user registers 0 through 15. Registers 16 through 31 are
reserved by `Script` for expression temporaries. Direct MetaIL supports all 32.
An invalid native-DSL register is a programming error and throws
`invalid_argument` at construction.

`Expr(int64_t)` constructs a DSLtk integer leaf. `Register` converts to an
expression leaf. Operators `+`, `-`, `*`, `/`, and `%` construct binary AST
nodes. `equal(a,b)` and `less(a,b)` construct comparisons, returning an IL
integer 0 or 1 when evaluated. Logical/bitwise operations and shifts can be
represented with explicit DSLtk AST tags or direct IL emission.

When both C++ operands are plain integers, C++ evaluates the expression before
the DSL sees it. Use `Expr(6) * Expr(7)` to force construction of an expression
tree. With a register operand, argument-dependent lookup finds the DSL
operators, so `x * 7` is also a DSL expression.

## Assignment and temporary allocation

```cpp
Script program;
program.set(x, 40)
       .set(y, 2)
       .set(x, x + y)
       .print(x)
       .halt(x);
auto compiled = program.finish();
```

Each `set` lowers an expression left-to-right into temporary registers and then
copies its result into the destination. This preserves source-register values
while evaluating an expression that reads its own destination.

A binary expression reuses its left temporary as the destination, so a left-deep
tree needs only two live temporaries regardless of length. What actually bounds
expression size is the AST depth limit of 256: a deeper value throws
`invalid_argument`, is caught by `set`, and becomes a retained diagnostic rather
than exhausting the C++ stack. Excess register pressure would likewise produce
a retained diagnostic. The failed assignment is transactional: it
does not leave half an expression in the IL builder. Other previously emitted
statements remain, but `finish()` reports the failure and supplies no valid
program through its success path.

`finish()` is const and does not consume the builder. It resolves symbolic
labels, verifies instructions, and returns `Outcome<Program>`. A script may be
extended after an earlier `finish()` call; the returned program is an owned
snapshot.

## Labels and conditional control flow

```cpp
Script factorial;
auto answer = reg(0), count = reg(1), one = reg(2);
factorial.set(answer, 1).set(count, 6).set(one, 1)
    .label("loop")
    .set(answer, answer * count)
    .set(count, count - one)
    .while_nonzero(count, "loop")
    .print(answer)
    .halt(answer);
```

`while_nonzero` emits a conditional branch; it is not a C++ loop construct and
does not automatically establish or close a scope. `jump_zero` branches when a
register is zero. `jump` is unconditional. Labels are local to one script.
Duplicate or unresolved labels are errors.

Use `emit(Instruction)` for operations not wrapped by a convenience method,
including calls, explicit pushes/pops, bitwise operations, and shifts. The
verifier still checks the resulting instruction fields.

## Inspecting and rewriting expression trees

```cpp
Expr expression = Expr(6) * Expr(7);
std::string diagnostic_tree = expression.tree.dump();
auto term = metachain::ast_to_term(expression.tree);
auto restored = metachain::term_to_ast(term);
```

The lossless bridge encodes a leaf as `leaf(tag, value)` and an inner node as
`node(tag, child...)`. Tags and values are atoms. This prevents a leaf with an
empty value from being confused with an empty inner node.

`ast_to_term` throws `invalid_argument` if the AST exceeds its depth bound;
`term_to_ast` returns an `Outcome` diagnostic instead. Both defaults are 256 and
both are caller-overridable. Keeping the two directions explicit matters because
a C++-owned value graph can already be deeper than any textual input limit.

The native expression lowering recognizes leaf tags `integer` and `register`,
and binary tags matching IL arithmetic/comparison mnemonics. Other AST forms
are rejected rather than ignored. For example:

```cpp
Expr shift(dsl::node<"shl">(
    dsl::leaf<"register">(0), dsl::leaf<"integer">(3)));
```

The caller owns ASTs and must not mutate them concurrently with lowering. AST
bridge recursion is intended for application-owned trees; use parser/rewrite
depth limits and avoid constructing adversarial deeply nested C++ values.

## Selecting an execution path

```cpp
auto p = factorial.finish();
metachain::Toolchain tc;
if (p) {
    auto interpreted = tc.run(*p);
    auto bytes = metail::encode(*p);
    auto assembly = tc.native_source(*p);
    auto executable = tc.executable(*p);
}
```

These are independent operations. `Script::finish()` verifies but does not run
the facade optimizer. `Toolchain::compile(text)` applies the optional optimizer;
for a script, use `metail::optimize` explicitly if desired. The optimized and
unoptimized programs preserve IL instruction indices and source-map entries.
