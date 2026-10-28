# 15. Lazy select and eager logical operations

This distinction is easy to miss when coming from C or C++: MetaIL's `select` is
lazy, but its logical `land` and `lor` operators are **eager**.

## Use select to guard work

<!-- check: juniper -->
```juniper
@fn(divide_or_zero, (numerator, divisor))
  @return(select(eq(divisor, 0), 0, div(numerator, divisor)))
@end
```

The zero-divisor arm does not evaluate the division. The signed-minimum/`-1` case
is still a possible trap; checking only for zero did not prove the operation
safe for all signed-64 inputs.

An effectful alternative is similarly conditional:

<!-- check: juniper -->
```juniper
@fn(read_if_enabled, (enabled))
  @return(select(enabled, device_read(), 0))
@end
```

`device_read()` occurs only when the condition is nonzero. The lowerer preserves
this by creating a branch, arm-local code, copies into a shared virtual result,
and generated labels. It does not eagerly evaluate both arms and then choose a
computed value.

## Logical operators do not short-circuit

<!-- check: juniper -->
```juniper
@fn(eager_logical, ())
  @return(land(0, tick()))
@end
@fn(lazy_logical, ())
  @return(select(0, ne(tick(), 0), 0))
@end
```

The first function still evaluates `tick()`. The second does not. Replacing the
first expression with `0` merely because its final boolean result is known would
lose an effect. The same eagerness applies to infix `&&` and `||` in this IR.

To model boolean short-circuiting while normalizing the result to `0`/`1`, use
these expression fragments:

```text
select(left, ne(right, 0), 0)  // short-circuit AND
select(left, 1, ne(right, 0))  // short-circuit OR
```

`left` is evaluated once; the selected arm determines whether `right` is
evaluated. The fragments assume `left` and `right` stand for expressions, not
macro arguments that will accidentally be duplicated elsewhere.

## Nested selects remain conditional

<!-- check: juniper -->
```juniper
@fn(read_mode, (enabled, fast))
  @return(select(enabled, select(fast, fast_read(), slow_read()), 0))
@end
```

Both arms are compiled, so their operations must be selectable even if one is
not executed for a particular input. Running a constant-folding pass first can
remove a statically unused arm; merely knowing a runtime condition does not.

Custom selection patterns cannot mention `select`, even inside another pattern.
That restriction prevents a backend rule from quietly changing lazy evaluation
into eager operand computation. Inspect [lazy.jun](examples/lazy.jun) with
`select` and `schedule` to see the explicit control-flow boundary.

[Previous](14-control-flow.md) · [Index](README.md) · [Next](16-effects-and-traps.md)
