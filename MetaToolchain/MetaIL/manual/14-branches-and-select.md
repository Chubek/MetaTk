# 14. Branches and selections

Use `br` to choose control-flow edges and `select` to choose an expression value.
Both treat zero as false and nonzero as true, but they occupy different positions
in the IR: a branch is a statement and `select` is a three-argument operator.

```cpp
auto x = metail::var("x");
metail::Function sign("sign", {"x"});
sign.ret(metail::select(x >= 0, 1, -1));
```

The same decision can be expressed using blocks:

```cpp
metail::Function sign("sign", {"x"});
sign.branch(x >= 0, "positive", "negative")
    .label("positive").ret(1)
    .label("negative").ret(-1);
```

`select` has lazy arm semantics: evaluating the IR chooses one arm rather than
evaluating both. Creating its C++ AST children is just construction, however;
host-language computations used to build those children still run normally.

Do not write `if (x >= 0)` expecting an IR branch. The comparison produces an
`Expr` with no boolean conversion, so this is rejected by C++. Likewise, the host
conditional operator cannot replace `metail::select`.

`BranchSimplification` can simplify branches with known constant conditions.
Expression rewrite kernels handle selected `select` patterns. Their guards
must still account for any discarded effects or traps.

[Contents](README.md) · [Next: Loops](15-loops-and-gcd.md)
