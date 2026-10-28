# 13. Mutable variables and statements

`let` assigns a mutable local. It does not create a new SSA definition whose name
can never be assigned again. This matters for hand-written IR, transformations,
and scheduling hazards.

## Reassignment is intentional

<!-- check: juniper -->
```juniper
@fn(accumulate, (x))
  @let(total, x)
  @let(total, add(total, 3))
  @let(total, mul(total, 2))
  @return(total)
@end
```

Each right-hand side observes the current local values before the assignment.
The lowerer produces a `COPY` into the named local after computing the expression.
You may also reassign a parameter; the same dependency rules apply.

## Statement vocabulary

<!-- check: juniper no-preprocess -->
```juniper
fn sample(x) {
entry:
  let current = sensor_read(x);
  eval audit(current);
  assume current >= 0;
  br current == 0, zero, nonzero;
zero:
  return 0;
nonzero:
  goto finish;
finish:
  return current;
}
```

* `let name = expr;` computes a value and assigns it.
* `eval expr;` evaluates an expression without keeping its result.
* `assume expr;` records an assumption expression; current passes preserve it
  without treating it as a general path-sensitive proof.
* `return expr;` terminates the function path.
* `goto label;` and `br condition, yes, no;` transfer control.
* `name:` labels a basic-block entry.

The example's calls are intentionally external placeholders; parsing does not
supply `sensor_read` or `audit`. Chapter 16 explains why they are effect barriers.

## An unused assignment is not necessarily dead work

<!-- check: juniper -->
```juniper
@fn(keep_observations, (x, divisor))
  @let(unused_pure, add(x, 1))
  @let(unused_call, tick())
  @let(unused_division, div(x, divisor))
  @return(0)
@end
```

The pure unused assignment can be eliminated. An optimizer must preserve the call
and possible division trap, for example by retaining them as `eval` statements.
Juniper delegates this work to MetaIL's actual dead-assignment implementation.

```sh
"$J" optimize "$WORK/unused.jun" --passes DeadAssignmentElimination \
  -o "$WORK/unused.optimized.junir"
```

## Initialize along every path

The validator does not prove definite assignment. The following pattern is a
programming mistake even if it passes structural checks:

```text
fn unsafe(condition) {
  br condition, initialize, skip;
initialize:
  let value = 7;
  goto done;
skip:
  goto done;
done:
  return value;  // No value was assigned on the skip path.
}
```

Initialize before the branch or assign on both paths. Do not treat a local's
absence from a declaration list as a request for zero initialization.

[Previous](12-integer-semantics.md) · [Index](README.md) · [Next](14-control-flow.md)
