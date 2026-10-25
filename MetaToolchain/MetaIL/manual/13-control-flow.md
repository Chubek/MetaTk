# 13. Labels and control-flow graphs

CFG construction starts a block at the first statement, at every label, and after
a `goto`, `br`, or `return` when more statements follow. Labeled blocks use the
label's name. Unlabeled blocks receive generated names such as `bb1`.

The last statement determines outgoing edges. `goto` has one target, `br` has its
true and false targets, and `return` has none. Otherwise execution falls through
to the next block. A branch whose two targets are equal has one distinct successor.

```text
fn choose(x) {
entry:
  br x, yes, no;
yes:
  return 1;
no:
  return 0;
}
```

Labels must be unique within a function, and branch targets must exist in that
function. The validator rejects unresolved targets even in blocks that are not
reachable from entry. Code after a terminator is represented as another block;
it is not silently discarded by parsing.

Use `ControlFlowGraph` to inspect successors, predecessors, statement positions,
and reachability. Statement positions in Lua-backed reports are one-based, while
the C descriptor arrays and C++ vectors use zero-based indexing.

For stable, readable analysis output, name important blocks explicitly and avoid
choosing labels that collide with generated `bbN` names.

[Contents](README.md) · [Next: Branches](14-branches-and-select.md)
