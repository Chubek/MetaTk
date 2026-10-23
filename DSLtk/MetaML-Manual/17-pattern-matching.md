# 17. Patterns, matches, and guards

Pattern matching analyzes structured data and introduces bindings. Alternatives follow `match value with`; each branch begins with `|` and separates its pattern from the body with `->`. `_` ignores a value without introducing a useful binding.

```metaml
describe xs := match xs with
  | [] -> "empty"
  | x :: _ when x < 0 -> "starts negative"
  | _ :: _ -> "nonempty"
Test.equal "empty match" (describe []) "empty"
Test.equal "guarded match" (describe [~2, 3]) "starts negative"
```

Supported patterns include literals, booleans, unit, variables, tuples, fixed lists, cons cells, constructors, records, and type annotations. A guard is evaluated only after the pattern matches and its bindings are available. A false guard moves to the next branch.

Branch order matters. Place specific patterns before catch-all patterns. A wildcard first branch makes later branches unreachable as a matter of runtime behavior, even when the checker does not reject every such case. Keep a deliberate fallback for data that may evolve.

Matching is eager in the scrutinee: it is evaluated once before alternatives are tested. Branch bodies are evaluated only after selection. This makes `match` preferable to eager helper arguments when one branch would perform an expensive or invalid computation.

A non-exhaustive match that reaches no alternative produces `M0401`. Function parameter patterns use the same matching machinery and can likewise fail at application time. Do not assume a successful static check proves exhaustive coverage of every nested pattern and guard.

```metaml
value_or_zero o := match o with | Some x -> x | None -> 0
Test.equal "constructor match" (value_or_zero (Some 7)) 7
```

For fallible public APIs, returning `Option` or `Result` usually communicates absence better than deliberately relying on a pattern-failure exception.

<!-- nav:begin -->
[Contents](README.md) · [Previous](16-records.md) · [Next](18-algebraic-data.md)
<!-- nav:end -->
