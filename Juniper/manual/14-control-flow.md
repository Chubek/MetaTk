# 14. Labels, branches, and loops

Juniper uses explicit basic-block control flow. There is no source `while`, `if`,
or `phi` construct; macros can generate such patterns, but the underlying IR is
labels, assignments, and transfers.

## A two-way branch

<!-- check: juniper -->
```juniper
@fn(maximum, (left, right))
  @br(ge(left, right), use_left, use_right)
@label(use_left)
  @return(left)
@label(use_right)
  @return(right)
@end
```

The branch tests its condition once. A nonzero result selects `use_left`.
Labels must be unique within a function, and all explicit targets must exist.
The first statement is the function entry; naming a label `entry` is a convention,
not an extra declaration requirement.

## A complete counted loop

<!-- check: juniper -->
```juniper
@module(loop_example)
@fn(sum_to_n, (n))
  @let(index, 0)
  @let(total, 0)
@label(test)
  @br(lt(index, n), body, done)
@label(body)
  @let(total, add(total, index))
  @let(index, add(index, 1))
  @goto(test)
@label(done)
  @return(total)
@end
```

For nonnegative `n`, the loop adds `0` through `n-1`, with wrapping accumulation.
For negative `n`, it exits immediately with `0`. Its state is represented by
mutable locals, not SSA induction values or hidden loop objects.

Use the provided equivalent [loop source](examples/loop.jun):

```sh
"$J" check "$MANUAL_EXAMPLES/loop.jun"
"$J" analyze "$MANUAL_EXAMPLES/loop.jun" \
  --passes ControlFlowGraph,Dominators,LiveVariables -o "$WORK/loop.analysis.json"
"$J" schedule "$MANUAL_EXAMPLES/loop.jun" -o "$WORK/loop.schedule.json"
```

## Be explicit about exits and fall-through

The MetaIL CFG recognizes fall-through between labeled blocks when a terminator
is absent. Use explicit `goto` in teaching examples and generated code when it
makes intended successors clearer. A `return`, `goto`, or `br` terminates the
current block. Statements that follow can be unreachable until another reachable
label is encountered; an optimizer may remove them.

An unresolved target is rejected:

<!-- check: juniper error=JUN-E100 -->
```juniper
fn missing_target() {
  goto nowhere;
}
```

## Scheduling is not loop optimization

The scheduler respects labels and transfers as full fences. Its cycle numbers
describe the conservative static instruction model; they are not a loop's dynamic
execution count. It does not unroll loops, software-pipeline iterations, estimate
trip counts, or move operations from one basic block into another.

**Practice:** add a second accumulator to the loop. Compare liveness before and
after an explicit MetaIL optimization pass, then inspect the selected copies that
update each mutable local.

[Previous](13-variables-and-statements.md) · [Index](README.md) · [Next](15-lazy-select.md)
