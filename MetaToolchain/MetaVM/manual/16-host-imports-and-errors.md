# Chapter 16 — Host imports, callbacks, and diagnostics

[Previous](15-cpp-embedding.md) · [Contents](README.md) · [Next](17-resource-limits-and-security.md)

## Host imports are explicit capabilities

Assembly declares `.import name arity`; the host registers that exact name
and arity before running. All imports resolve before execution, even unused
ones. The VM never discovers dynamic libraries or casts arbitrary native
function signatures into a guest ABI.

```c
typedef mvm_status (*mvm_native)(mvm_vm *vm,
    const mvm_value *arguments, size_t count,
    mvm_value *result, void *user, mvm_error *error);
```

Arguments are borrowed and rooted for the callback. The returned value must
be a scalar or live object belonging to this VM. User data remains host-owned.
Callbacks may allocate, collect, and create explicit roots, but their own
temporary objects need pins across later allocations. They may not destroy
or reenter the running VM or mutate its running module.

The complete [host example](examples/host.c) registers an `increment` import,
validates its argument type, and returns an integer. It illustrates the normal
status-return boundary without assuming guest input is well typed.

## Single and atomic registration

`mvm_register_native` copies the name and borrows callback/user pointers.
Duplicate names and registration during execution fail. There is no unregister
operation implied by the registry.

`mvm_register_natives(vm, bindings, count, &error)` installs a batch atomically.
Invalid entries, duplicates within the batch or existing registry, allocation
failure, and busy VMs leave prior bindings unchanged. Zero entries are a no-op
only for a valid idle VM. `Graphics::bind` uses this API so failure cannot leave
half a graphics interface installed.

## Status reference

| Status | Interpretation |
|---|---|
| `MVM_OK` | Success |
| `MVM_INVALID` | Invalid API argument/state |
| `MVM_OOM` | Allocation failure |
| `MVM_LIMIT` | Configured resource ceiling exhausted |
| `MVM_IO` | File/stream operation failed |
| `MVM_FORMAT` | Malformed encoded/text input |
| `MVM_VERIFY` | Module structural verification failed |
| `MVM_TYPE` | Incompatible value/object kind |
| `MVM_BOUNDS` | Invalid index/range |
| `MVM_DIV_ZERO` | Integer division/remainder by zero |
| `MVM_NOT_FOUND` | Missing name or unsupported mapped identity |
| `MVM_HOST_ERROR` | Host callback failure at the run boundary |
| `MVM_BUSY` | Forbidden mutation/reentrancy while running |

`mvm_status_string` supplies a diagnostic name. `mvm_error_clear` initializes
an optional diagnostic. `mvm_error` includes status, a 256-byte message, and
function/instruction/line coordinates. Function and instruction are zero-based;
`SIZE_MAX` denotes unknown. Line numbers are one-based, with zero meaning absent.

## Exception and side-effect boundaries

A callback must return normally. C++ callbacks must catch standard and unknown
exceptions. Machinery normalizes a failing callback to `MVM_HOST_ERROR` at
`mvm_run`, preserves its detailed message, and attaches the guest host-call
site. For example, DVI replay reports allocation failure internally as
`MVM_OOM`, but the run sees `MVM_HOST_ERROR` with an allocation diagnostic.

Unwinding frames does not undo a printed line, file write, pixel mutation, or
network request performed by the host. Design idempotency, authorization,
cancellation, and rollback at the application layer.

**References:** [CBridge.h](../Machinery/CBridge.h), [DVI callback handling](../vmweave/DviReplay.hpp).
