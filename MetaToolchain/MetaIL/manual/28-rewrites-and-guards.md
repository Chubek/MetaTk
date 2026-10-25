# 28. Declarative rewrites and guards

A rewrite kernel registers a structural pattern and replacement:

```lua
local K = lkopt.optimizer.new "ProjectMultiplyZero"
K:rewrite {
  pattern = "mul($x,0)",
  replace = "0",
  guard = "pure:x"
}
```

Captures bind complete expression subtrees. Repeated captures require structural
identity. Every replacement capture must already appear in the pattern; this
prevents introducing an unbound subtree. Rewrites are applied through expression
traversal rather than by textual search-and-replace.

Guard clauses are separated by semicolons, with comma-separated capture names
after the colon: `pure:x,y;const:c`. Supported guards are `const`, `nonzero`,
`positive`, `nonnegative`, `power-of-two`, `pure`, and `boolean`.

The numeric predicates currently require literal integers; they are not queries
to a general inferred-range analysis. `boolean` requires a literal 0 or 1. `pure`
includes freedom from observable traps, not merely absence of calls.

If a rewrite drops or duplicates a captured subtree, guard that subtree for
purity. If it reorders effectful operands, require their purity too. Algebraic
identities must hold under signed-64 wraparound, not only real arithmetic.

The loader rejects malformed guards, unbound captures, and identical pattern/
replacement no-ops. The bundled verifier additionally checks boundary values,
call traces, traps, and that every declarative rule actually fires.

[Contents](README.md) · [Next: Procedural passes](29-procedural-passes.md)
