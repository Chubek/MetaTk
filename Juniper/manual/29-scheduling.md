# 29. Scheduling, latency, and resource capacities

The scheduler is a deterministic critical-path list scheduler over a conservative
dependency graph. It considers issue width, launch capacity per resource, and
result latency. These are model parameters, not measured CPU performance data.

## Expose independent work

<!-- check: juniper mode=schedule opcodes=MUL,ADD,RET -->
```juniper
backend dual_multiply extends portable {
  issue 2;
  resource alu 2;
}
fn pair_product(a, b, c, d) {
  return add(mul(a, b), mul(c, d));
}
```

In this profile both independent multiplies can issue at cycle 0, because issue
width and `alu` capacity are both 2. Their portable latency is 3, so the consuming
add cannot issue before cycle 3. The return is a full control fence and follows
the add. This is a static schedule, not a dynamic benchmark result.

Compare with `resource alu 1;`: the machine is still two-wide overall, but only
one instruction using `alu` may launch per cycle. Increasing issue width alone
does not increase a saturated resource's capacity.

## Resource occupancy is per launch cycle

A latency-4 instruction does not reserve its resource for four cycles in this
model. It occupies one launch slot at its issue cycle and makes its result
available after four cycles. Nonpipelined units with occupancy greater than one
cycle are not separately modeled. Do not interpret capacity 1 as automatically
implementing a nonpipelined multiplier.

The ready instruction with the longest remaining critical path has priority;
ties use original instruction IDs. Equal-cost opcode selection already happened
earlier and is not revisited by scheduling.

## Validate the schedule as data

The supplied [inspect_schedule.py](examples/inspect_schedule.py) checks issue
width, resource capacities, stable-ID dependencies, and producer latencies:

```sh
"$J" schedule "$MANUAL_EXAMPLES/parallel.jun" -o "$WORK/parallel.json"
python3 "$MANUAL_EXAMPLES/inspect_schedule.py" "$WORK/parallel.json"
```

The essential dependency check looks like this Python fragment:

<!-- check: python -->
```python
def check_latencies(function):
    by_id = {inst["id"]: inst for inst in function["instructions"]}
    for inst in function["instructions"]:
        assert inst["cycle"] is not None
        for dependency in inst["dependencies"]:
            producer = by_id[dependency]
            assert inst["cycle"] >= producer["cycle"] + producer["latency"]
```

Do not index the reordered instruction list directly with a dependency ID. Build
the ID map, as above.

## Boundaries to remember

The current scheduler cannot cross effect/control/trap fences or basic-block
boundaries. Structural pseudos, including labels, conservatively consume one
cycle and an issue slot. Schedule length therefore includes costs that an
eventual physical backend may model differently.

`schedule(machine)` returns a new value and does not modify its input. Scheduling
an already scheduled module reconstructs original ID order before scheduling;
with unchanged metadata, output is deterministic. It does not consume manually
inserted dependency hints as a custom graph: dependencies are recomputed from
the instruction definitions, uses, and barriers.

[Previous](28-machine-ir.md) · [Index](README.md) · [Next](30-hazards-and-fences.md)
