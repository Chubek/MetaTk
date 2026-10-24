# 12. Local bindings and recursion

Local `let` expressions bind names for a following body introduced by `in`. Use `let rec` when a local function calls itself. Top-level function definitions support recursion through their checked binding cells; neighboring definitions may be analyzed together where needed.

```metaml
sum_to n :=
  let rec loop k acc := if k <= 0 then acc else loop (k - 1) (acc + k) in
  loop n 0
Test.equal "tail recursion" (sum_to 100) 5050
Test.equal "local value" (let x := 20 in x * 2 + 2) 42
```

The evaluator optimizes supported tail calls by replacing the active expression and environment instead of growing a host call stack. This makes accumulator-style traversals suitable for long lists. It does not make every recursive operation stack safe: non-tail recursion, nested structural equality, and deeply nested syntax still have resource costs.

Write the recursive call as the final computation in a branch. `loop next acc` can be a tail call; `1 + loop next acc` must return before the addition and is not the same shape. The portable List implementation uses reversed accumulators and iterative host operations to avoid many such pitfalls.

An embedding can set a recursion-depth limit using `set_max_depth`. This is not a time quota or a complete defense against hostile programs. A tail-recursive infinite loop can continue indefinitely without exceeding recursion depth. Likewise, a small expression can request substantial allocation through library operations.

The supplied REPL uses a large stack on supported POSIX systems for non-tail recursive workloads. Embedded applications should not assume that wrapper policy applies to their own threads. Set conservative limits and isolate untrusted evaluation in a subprocess with operating-system resource controls.

**Exercise:** implement factorial with an accumulator, then compare its trace with a non-tail-recursive definition for small inputs.

<!-- nav:begin -->
[Contents](README.md) · [Previous](11-type-inference.md) · [Next](13-tuples.md)
<!-- nav:end -->
