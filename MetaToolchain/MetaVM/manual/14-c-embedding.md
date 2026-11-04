# Chapter 14 — Embedding through the C API

[Previous](13-objects-and-garbage-collection.md) · [Contents](README.md) · [Next](15-cpp-embedding.md)

## The ownership-first sequence

1. Obtain a verified module by compilation, loading, a factory, or direct building.
2. Create a VM with chosen limits.
3. Register all required host imports before execution.
4. Allocate/root object arguments and locate the entry function.
5. Run, inspect the status and result kind, then copy or pin needed results.
6. Release roots while their VM is alive; destroy the VM and module.

[examples/embed.c](examples/embed.c) is a complete C11 program with cleanup on
every failure path. Build it against `MetaVM::Machinery`; the core has no C++
runtime requirement. Include `MetaVM.h` using the include directories exported
by that target rather than inventing absolute header paths.

## A minimal call inside an embedding

```c
uint32_t entry;
mvm_value result = mvm_nil();
mvm_status status = mvm_module_find(module, "main", &entry);
if (status == MVM_OK)
    status = mvm_run(vm, module, entry, NULL, 0,
                     MVM_ENGINE_BYTECODE, &result, &error);
if (status == MVM_OK && result.kind == MVM_INT) {
    /* result.as.integer is now meaningful. */
}
```

The module and VM above must already be valid. Lookup failures return a status;
`mvm_module_find` does not itself accept an error object. Do not print a stale
error message from an earlier operation when diagnosing that lookup.

## Output and lifetime reference

| Value/resource | Ownership |
|---|---|
| Compiled/loaded/cloned/factory module | Caller; `mvm_module_destroy` |
| VM from `mvm_create` | Caller; `mvm_destroy` |
| Module name/code view | Borrowed until module mutation/destruction |
| Heap object | VM; keep live with traced reachability/root |
| `mvm_root` | Handle released with `mvm_unpin` before VM destruction |
| Native callback `user` data | Caller; must outlive its registered uses |
| `mvm_buffer` | Caller initializes `{0}` and disposes |
| Path from `mvm_path_join` | Caller; release with `free` |

Core APIs report status codes rather than aborting, exiting, or using
`longjmp`. Optional `mvm_error*` outputs may be null. Module-producing APIs
clear their output on failure; run outputs become nil. Simple accessors
generally leave outputs unchanged on failure, so never read them without
checking the status.

## Adopting generated C factories

A generated factory has signature `mvm_status name(mvm_module**, mvm_error*)`.
It creates a module; it does not create a VM or run `main`. Link its `.c`
translation unit and Machinery exactly once, call the factory, then follow
the normal embedding sequence. Graphics imports and application imports still
need registration. Factories are useful for shipping verified instruction
bytes without invoking an assembly parser at application startup.

For multi-threaded hosts, share only immutable modules and give workers their
own VMs, or explicitly synchronize a shared VM. C objects and callback pointers
have normal application lifetimes; the VM cannot repair a host use-after-free.

**References:** [complete example](examples/embed.c), [status reference](16-host-imports-and-errors.md).
