# 10. Calls, effects, and traps

Calls such as `tick()` are conservatively effectful. They may not be silently
discarded, duplicated, or reordered by an algebraic rewrite. The language records
calls and their arguments, but does not currently resolve an executable native
callee or enforce a call signature.

Division and remainder truncate toward zero. Division by zero and the signed
minimum divided by -1 are treated as observable invalid/trapping cases by the
optimizer's contract. Shift counts outside 0 through 63 are likewise not folded.
The implementation must preserve these expressions when eliminating them would
hide an observable invalid operation.

```text
fn effects() {
  let unused = tick() * 0;
  let trap = (1 / 0) * 0;
  return 3;
}
```

Multiplying either subtree by zero does not make it safe to erase. Dead assignment
cleanup can retain its evaluation as an `eval` statement. The `pure` guard used
in rewrites means safe to discard, duplicate, or reorder, including trap behavior.

`select(condition,yes,no)` is special: only the chosen arm is evaluated in the
IR semantics. Its C++ builder constructs all three AST children but does not run
their represented calls. Logical `land` and `lor` are integer operators and are
not a substitute for C++ host-language short-circuit control flow.

[Contents](README.md) · [Next: Modules and functions](11-modules-and-functions.md)
