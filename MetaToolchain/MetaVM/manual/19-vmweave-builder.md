# Chapter 19 — The typed VMWeave Builder

[Previous](18-serialization-io-and-cache.md) · [Contents](README.md) · [Next](20-definition-ir.md)

## Purpose and boundary

`vmweave::Builder`, in `vmweave/Builder.hpp`, is a C++17 authoring layer over
Machinery assembly. It accumulates source and finalizes through Machinery's
real compiler/verifier. It is not a guest interpreter, independent parser,
or native backend. Labels and call references still resolve at finalization.

```cpp
vmweave::Builder builder;
builder.function("main", 0, 3);
builder.constant(0, 6);
builder.constant(1, 7);
builder.binary("mul", 2, 0, 1);
builder.ret(2);
builder.end();
auto module = builder.finish();
```

This creates a normal zero-argument Machinery module returning `42`. A complete
Builder and Definition host is provided in [examples/authoring.cpp](examples/authoring.cpp).

## Method reference

| Method | Contract |
|---|---|
| `Builder(limit)` | Default source ceiling 16 MiB |
| `function(name, arity, registers)` | Open a function; reject nested opens/bad counts |
| `constant(dst, value)` | Emit signed-i64 constant to an in-range local |
| `binary(op, dst, left, right)` | Emit an accepted integer/comparison operation |
| `label(name)` | Emit a validated local label |
| `jump(name)` | Unconditional branch to a label |
| `jump_false(reg, name)` | Machinery false-value branch |
| `ret(reg)` | Return an in-range local |
| `end()` | Close an open function |
| `emit(line)` | Raw bounded source emission |
| `source()` | Borrow the accumulated source string |
| `finish()` | Compile and verify; reject an unterminated typed function |

Accepted typed binary names are `add`, `sub`, `mul`, `div`, `mod`, `eq`, `lt`,
`le`, `and`, `or`, `xor`, `shl`, and `shr`. Typed operations require an open
function and validate register ranges. Failed validation leaves prior source
unchanged. Source emission rejects embedded NUL and independently checks the
configured byte budget, including appended newlines.

## Mixing typed and raw construction

`emit` exposes the assembly bootstrap for instructions without dedicated typed
methods, such as arrays, host calls, and import declarations. Raw emission does
not update the Builder's typed structural state. Pair typed `function`/`end`
when using typed operations, and do not raw-emit an extra `.end` or function
declaration expecting the typed state tracker to notice it.

Raw text may defer structural errors until `finish`; source access alone is
not verification. If the authoring layer translates a higher-level compiler,
keep its own source-to-output map for diagnostics. The finalized binary does
not retain the assembly compiler's line information.

## Correct conditional lowering

`jump_false` implements Machinery truthiness. To loop until an integer counter
reaches zero, first use `binary("eq", ...)` against an explicit zero constant,
then branch on the resulting boolean. Passing a scalar zero directly is a
common infinite-loop bug. The supplied typed factorial authoring example tests
this boundary and returns `3628800`.

The Builder owns its source; the returned `Module` separately owns the compiled
module. Do not mutate a shared Builder concurrently. Its source limit is a
build-time allocation ceiling, not a substitute for VM execution limits.

**References:** [Builder.hpp](../vmweave/Builder.hpp), [MetaML authoring](22-metaml-authoring.md).
