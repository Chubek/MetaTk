# 8. Macro parameters, grouping, and hygiene

The three recurring macro errors are splitting an argument at a comma,
duplicating an effectful expression, and accidentally capturing a local name.
Juniper's macro layer is not hygienic; design your macros accordingly.

## Keep comma-containing arguments grouped

This is a complete function declaration:

<!-- check: juniper -->
```juniper
@fn(sum_three, (a, b, c))
  @return(add(add(a, b), c))
@end
```

The parameter list `(a, b, c)` is the second argument to `@fn`. Passing
`@fn(sum_three, a, b, c)` is not equivalent. Nested expression parentheses also
protect their internal commas:

<!-- check: juniper -->
```juniper
#define @twice mul(#1, 2)
@fn(transform, (x, y))
  @return(@twice(add(x, y)))
@end
```

For the same reason a pattern such as `add(reg, imm(-8, 7))` fits in one argument
to `@rule`. Arbitrary braces/brackets are not a substitute for the profile's
parenthesis grouping when designing your own macros.

## Make evaluation count explicit

<!-- check: juniper -->
```juniper
#define @square_value mul(#1, #1)
@fn(sample_energy, ())
  @let(sample, tick())
  @return(@square_value(sample))
@end
```

Here `tick()` is evaluated once by the IR, then the local is read twice. By
contrast, substituting `tick()` directly into `@square_value` creates two calls.
GPPX itself is not executing those calls; it generates source containing them.

Macros that introduce temporaries should accept names from the caller:

<!-- check: juniper -->
```juniper
#define @square_into let #1 = mul(#2, #2);
@fn(pair_energy, (left, right))
  @square_into(left_squared, left)
  @square_into(right_squared, right)
  @return(add(left_squared, right_squared))
@end
```

This is easier to audit than a macro that repeatedly assigns an undocumented
`temporary`. The caller must still provide names that do not collide with live
locals. There is no automatic gensym facility in the Juniper standard macros.

## Naming and review conventions

* Use expression macros only where an expression is expected.
* Use `*_into(destination, ...)` for macros that assign a caller-named local.
* Reserve a project prefix for reusable macro names and any introduced locals.
* Document whether an argument is read once, repeated, or used as a name.
* Avoid redefining standard macros such as `@fn` in ordinary application files.

For a large generated transformation, construct or edit `juniper::Expr` values
instead. Native IR makes node kinds and evaluation order explicit, and validation
can run before emission. Chapter 22 shows a traversal that does not duplicate
calls or turn a lazy select into eager work.

**Practice:** expand `@square_value(tick())`, count `tick` occurrences in the
generated MetaIL, then compare with `sample_energy`. This is a source-inspection
exercise, not a request to execute an unavailable Juniper interpreter.

[Previous](07-macro-basics.md) · [Index](README.md) · [Next](09-includes.md)
