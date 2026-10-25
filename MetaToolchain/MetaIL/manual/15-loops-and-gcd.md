# 15. Loops and Euclid's algorithm

Loops are ordinary back edges in the CFG. They do not have a separate syntax or
an implicit induction variable. Euclid's algorithm for nonnegative inputs is a
compact example of mutable locals and a loop-carried assignment:

```cpp
using namespace metail;
auto a = var("a"), b = var("b");
Module module("euclid");
module.function("gcd", {"a", "b"})
    .label("entry").jump("loop")
    .label("loop").branch(b != 0, "step", "done")
    .label("step")
    .let("remainder", a % b)
    .let("a", b)
    .let("b", var("remainder"))
    .jump("loop")
    .label("done").ret(a);
```

The remainder is computed before overwriting `a`; reversing these assignments
would change the algorithm. Expressions store variable names, so `a` and `b` in
the builder refer to their values at the represented point of execution.

Analyze the resulting module with `NaturalLoops`, `Dominators`, and `LiveVariables`
to inspect the back edge, header, and loop-carried uses. Natural-loop analysis
reports structural loop information; it does not perform automatic loop unrolling
or prove termination.

The executable example `metail_example_gcd` constructs, optimizes, and prints this
kind of IR. It does not compile or run a native GCD routine. A host C++ `for` loop
around builder calls would generate repeated statements, not a runtime IR loop.

[Contents](README.md) · [Next: C contexts](16-c-contexts.md)
