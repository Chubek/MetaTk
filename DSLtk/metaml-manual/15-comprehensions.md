# 15. List comprehensions

A comprehension combines generators, optional guards, and an output expression. It is an eager way to construct a list, not a lazy stream. A generator's source is evaluated in the environment available at that point, including bindings from earlier generators.

```metaml
squares := [x * x for x <- List.range 0 6 if x % 2 == 0]
pairs := [(x, y) for x <- [1, 2], y <- [10, 20]]
Test.equal "guarded comprehension" squares [0, 4, 16]
Test.equal "nested generators" pairs [(1, 10), (1, 20), (2, 10), (2, 20)]
```

Generators run in source order, like nested loops, and results preserve that order. The last generator varies fastest in a Cartesian product. Guards are boolean expressions; a false guard discards the current combination before later work proceeds.

Generator patterns can unpack tuples and constructors. A source element that does not match the pattern is skipped rather than producing the ordinary function-parameter mismatch diagnostic. This makes a comprehension useful for selecting successful constructor values, but the behavior should be explicit in code review.

```metaml
present := [x for Some x <- [Some 1, None, Some 3]]
Test.equal "pattern generator" present [1, 3]
```

Each successful combination contributes one output element. If you need a flat-map operation, generate the nested elements explicitly or use `List.flat_map`. A comprehension over two lists can allocate the product of their lengths; do not estimate memory by the number of generator clauses alone.

Keep effects out of output expressions and guards unless their order is intentionally part of the program. For fallible mapping that stops at the first failure, prefer `Option.traverse` or `Result.traverse`. A comprehension that silently drops failures is a different operation.

**Check:** compare a comprehension with an equivalent `List.filter` followed by `List.map` on a small finite input.

<!-- nav:begin -->
[Contents](README.md) · [Previous](14-list-syntax.md) · [Next](16-records.md)
<!-- nav:end -->
