# Chapter 17 — Resource limits and security boundaries

[Previous](16-host-imports-and-errors.md) · [Contents](README.md) · [Next](18-serialization-io-and-cache.md)

## Machinery defaults

Create a policy by modifying `mvm_default_limits()`, then pass it to
`mvm_create`. All fields must be nonzero.

| Field | Default | Scope |
|---|---:|---|
| `max_frames` | 1,024 | Simultaneously active call frames |
| `max_registers` | 1,048,576 | Aggregate active register windows |
| `max_heap_bytes` | 67,108,864 | Object headers and payloads |
| `max_module_bytes` | 16,777,216 | Module/input/instruction-byte checks |
| `max_steps` | 10,000,000 | Instructions per invocation across all calls |

```c
mvm_limits limits = mvm_default_limits();
limits.max_frames = 64;
limits.max_registers = 65536;
limits.max_heap_bytes = 8 * 1024 * 1024;
limits.max_steps = 100000;
```

A fuel limit alone does not cap memory. A short program may allocate a large
array; a recursive program may grow frames and registers before its fuel
expires. Bound each resource independently and account for host-side data.

Loader byte limits are separate arguments: zero means “use default,” not “read
zero bytes.” Native-profile `Limits` classes often treat zero as a literal
ceiling. Do not reuse a generic “zero means unlimited” configuration convention
across the project.

## What these budgets do not measure

Machinery's heap count is not process RSS. Modules, parser tables, names,
bindings, roots, frame/predecode storage, and allocator overhead have separate
costs. The cumulative array/environment budgets in native subsets likewise
count logical elements/cells, not exact bytes. Returned snapshots may copy
program data. Host metric providers and rendering sinks can allocate outside
guest budgets.

Choose conservative input caps before parsing, shape caps before building
large tables, execution fuel, allocation ceilings, and application-level
deadlines. For strong isolation from hostile tenants, add an operating-system
process boundary and resource policy; the library is not such a boundary.

## Trusted authoring versus untrusted guest input

The actual MetaML evaluator runs trusted build-time programs. Its step/depth
checkpoints do not sandbox imports, filesystem access, arbitrary host effects,
or computation inside a registered primitive. Similarly, a VM instruction
budget does not preempt an application callback.

Guest format validation is still essential: reject unknown opcodes, malformed
lengths, invalid branches, and unsupported versions. Never “recover” by choosing
a different catalog identity or emitting a generic arithmetic program. A
plausible but incorrect guest result is worse than a clear unsupported-format
diagnostic.

## Host capability checklist

- Register only imports the guest is authorized to use.
- Validate callback argument types, ranges, and allocation sizes.
- Avoid passing privileged device handles or ambient paths by accident.
- Keep callback state alive and synchronize mutable shared state.
- Bound metric lookup, rendering, I/O, and callback work independently.
- Treat direct filesystem helpers as ordinary path operations, not sandboxed I/O.
- Preserve failure diagnostics without mistaking them for rolled-back effects.

DRMVM receives an authorized fd rather than searching for a display. DVI treats
special commands as opaque bytes rather than executing driver instructions.
These capability boundaries should remain explicit in application integration.

**References:** [native limits](29-ssavm-execution.md), [closure budgets](34-secd-machine.md), [DVI](36-dvi-and-replay.md).
