# 30. Mutable-register hazards and full fences

Because source locals are mutable, true dependencies alone are insufficient.
Juniper's scheduler models RAW, WAR, and WAW relationships as well as full fences.

## Read after write: preserve the producer

<!-- check: juniper mode=schedule -->
```juniper
fn raw(x) {
  let value = mul(x, 3);
  return add(value, 7);
}
```

The add must observe the assigned `value`, not issue before the producing
computation/copy. This is a RAW dependency: **read after write**.

## Write after read: preserve the old value

<!-- check: juniper mode=schedule -->
```juniper
fn war(x) {
  let old_use = add(x, 2);
  let x = 9;
  return add(old_use, x);
}
```

The first add reads the original parameter `x`. Moving the overwrite before that
read would change the result. The scheduler adds WAR ordering, also called an
anti-dependency, even though the later write does not produce the old value.

## Write after write: preserve final state

<!-- check: juniper mode=schedule -->
```juniper
fn waw(x) {
  let value = add(x, 1);
  let value = mul(x, 2);
  return value;
}
```

The second assignment must remain the last write to `value`. A prior optimizer
may legitimately remove the unused pure first assignment; the scheduler does
not perform that dead-code transformation itself.

## Fences are stronger than one register dependency

<!-- check: juniper mode=schedule opcodes=CALL,DIV -->
```juniper
fn fences(a, b, divisor) {
  let first = mul(a, b);
  eval notify();
  let second = div(first, divisor);
  return add(second, 1);
}
```

`notify` is an effect fence, division is a possible-trap fence, and return is a
control fence. A full fence waits for preceding work in its region and prevents
later work crossing it. Labels, branches, jumps, assumptions, and calls also
provide such boundaries. Unknown physical alias relationships or arbitrary
memory operations are not modeled by guessing from opcode names.

## When constructing machine instructions yourself

Keep IDs unique and contiguous from zero within a function. Set valid resources
and positive latencies. Reserved structural control/effect opcodes must have
`barrier = true`; the scheduler rejects a `CALL` that falsely claims otherwise.
For custom instructions, **you** must accurately declare effects and barriers.
The scheduler cannot infer that an arbitrary `DEVICE_WRITE` opcode has effects.

This C++ fragment illustrates an explicitly fenced custom operation:

```cpp
juniper::Instruction instruction;
instruction.id = 0;
instruction.opcode = "DEVICE_WRITE";
instruction.resource = "control";
instruction.latency = 1;
instruction.barrier = true;
instruction.operands = {{juniper::Operand::Kind::reg, "value", 0}};
```

Inserting it is an application-defined machine-IR operation, not a source-language
extension or an automatically encodable hardware instruction. Renumber after
structural edits if you will reschedule, and retain explicit tests of register
reads/writes and effect ordering.

[Previous](29-scheduling.md) · [Index](README.md) · [Next](31-cpp-pipeline.md)
