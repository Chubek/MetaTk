# Chapter 12 — Execution engines, frames, and concurrency

[Previous](11-verification-and-optimization.md) · [Contents](README.md) · [Next](13-objects-and-garbage-collection.md)

## Starting an invocation

```c
mvm_vm *vm = mvm_create(NULL, &error);
/* Check vm before use. */
mvm_status status = mvm_run(vm, module, entry, arguments, count,
                            MVM_ENGINE_BYTECODE, &result, &error);
```

`NULL` limits select defaults. Explicit limits must have nonzero fields.
The function index is not a name; resolve names with `mvm_module_find`.
Argument count must match the entry function. Object arguments must belong
to the same VM and remain properly rooted while host code prepares them.

`mvm_run_bytecode` and `mvm_run_threaded`, declared in the engine-specific
headers, provide convenience entry points. `mvm_run` accepts the engine enum
directly and is the common embedding interface.

## Engine implementation and guarantees

| Engine | Dispatch preparation |
|---|---|
| `MVM_ENGINE_BYTECODE` | Switch-based dispatch over instructions |
| `MVM_ENGINE_THREADED` | Per-invocation predecode into C handler pointers |

Both use the same instruction semantics and an iterative dispatcher. The
threaded engine is not a native JIT, operating-system thread pool, or
GNU computed-goto implementation. It does not require writable executable
memory or reinterpret data pointers as function pointers.

Predecoded storage belongs to the invocation. An immutable module has no
hidden lazy dispatch cache. A module cache is a different facility described
in chapter 18. Measure both engines with representative invocation sizes:
threaded preparation can outweigh dispatch savings on small programs.

## Frames and budgets

Calls use explicit VM frames rather than recursive C calls. Register windows
are private; limits bound both active frame count and aggregate active
registers. Every instruction, including calls, returns, and NOPs, consumes one
step from the invocation-wide budget. Recursive functions do not receive fresh
fuel on each call. `mvm_steps(vm)` reports the VM's instruction accounting.

Host-callback computation is not charged one VM step per host operation.
A callback that loops forever is not stopped merely because the calling
instruction consumed fuel. The host must impose its own work/deadline policy.

## Failure recovery and result lifetime

Execution failure unwinds all frames, sets the result output to nil, and leaves
the VM reusable. The returned status is authoritative; diagnostics add context.
Do not treat a partially observed host effect as rolled back by VM unwinding.

The most recent successful result is rooted by the VM until the next invocation
starts. Pin it explicitly if it must survive a later run. Validation failures
before invocation setup do not release the previous result, but relying on that
distinction instead of explicit pinning makes application lifetimes fragile.

## Concurrency policy

Independent VMs may execute the same immutable module concurrently. Synchronize
every operation on a particular VM or cache. Module builders, replacement,
optimization, and destruction require exclusive access. A native callback may
not reenter or destroy its running VM; reentrant execution returns `MVM_BUSY`.
Neither the wrapper nor the native-profile APIs create a scheduler for you.

**References:** [ExecutionEngine.h](../Machinery/ExecutionEngine.h), [callbacks](16-host-imports-and-errors.md).
