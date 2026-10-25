# 12. Statements and mutable locals

The statement vocabulary is deliberately small:

| Text | Meaning | C++ builder |
|---|---|---|
| `let x = e;` | Assign a local | `let("x", e)` |
| `return e;` | Return a value | `ret(e)` |
| `eval e;` | Evaluate for observable behavior | `eval(e)` |
| `assume e;` | Record an assumption expression | `assume(e)` |
| `name:` | Mark a basic-block label | `label("name")` |
| `goto name;` | Unconditional edge | `jump("name")` |
| `br e, yes, no;` | Conditional edges | `branch(e,"yes","no")` |

`let` assigns mutable locals. Repeated names are legal, including assignments to
parameters; this is not SSA. Expression construction captures a variable's name,
not a particular defining assignment.

```text
fn increment_twice(x) {
  let x = x + 1;
  let x = x + 1;
  return x;
}
```

The canonical function representation is an ordered statement list. Labels and
terminators determine basic blocks; there is no separate public block container.
This makes it straightforward to insert or replace statements programmatically.

`assume` is represented and analyzed as an expression-bearing statement. Do not
infer that it provides a general theorem prover or a sophisticated range lattice
to all optimizations. Only documented, implemented pass behavior applies.

[Contents](README.md) · [Next: Control flow](13-control-flow.md)
