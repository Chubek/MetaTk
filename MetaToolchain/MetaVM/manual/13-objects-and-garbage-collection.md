# Chapter 13 — Objects, garbage collection, and roots

[Previous](12-execution-engines.md) · [Contents](README.md) · [Next](14-c-embedding.md)

## Built-in heap objects

Machinery supplies immutable byte strings and mutable arrays of tagged values.
Strings can contain embedded NUL bytes; their length is a byte count, not a
Unicode character count. Arrays initially contain nil. There is no public
general-purpose user-object layout or moving-object API.

| API | Operation |
|---|---|
| `mvm_string_new` | Copy a byte span into a VM-owned string |
| `mvm_array_new` | Allocate a nil-initialized array |
| `mvm_object_type`, `mvm_object_length` | Query a live same-VM object |
| `mvm_string_data` | Borrow immutable bytes and length |
| `mvm_array_get`, `mvm_array_set` | Bounds-checked element access |
| `mvm_object_describe` | Read immutable built-in layout metadata |

Arrays accept scalars and live references belonging to that VM. Foreign-VM
objects are rejected. Application code must still supply valid accessible
pointer/length arguments; API validation cannot make arbitrary C pointers safe.

## Root before allocating again

The collector is non-moving, iterative mark/sweep and follows array cycles.
It traces active frame registers, explicit roots, and the current successful
run result. A local C variable holding `mvm_value` is **not** a GC root.

```c
mvm_value array;
mvm_root *root = NULL;
mvm_status status = mvm_array_new(vm, 4, &array);
if (status == MVM_OK) status = mvm_pin(vm, array, &root);
/* Only after successful pinning may later allocations safely occur. */
/* ... use array ... */
mvm_unpin(root);
```

Check both statuses before use. `mvm_pin` does not collect. Each successful
pin creates a handle that must be released; `mvm_root_value` reads its value.
`mvm_unpin(NULL)` is harmless, but a released root cannot be used again.

This rule also applies to borrowed string bytes. If those bytes back a later
`mvm_string_new` input, pin the original string before the allocation, because
allocation can collect it while the bytes are still needed.

## Collection triggers and accounting

Allocation attempts collection when the configured heap-byte limit would be
exceeded. `mvm_collect(vm)` explicitly collects and returns the reclaimed object
count. A non-moving collector preserves addresses only while objects remain
alive. Freed addresses can be reused; a stale unrooted value cannot be made
valid by comparing its address to a newly allocated object.

`mvm_memory_usage` reports object count, heap bytes, and roots. Heap bytes
include object headers/payloads, not all process allocations. Modules, parser
tables, root records, bindings, frames, and predecode storage are not one global
heap quota. Diagnose growth using both VM statistics and process measurements.

## Destruction and C++ lifetime differences

Destroying the C VM releases its objects and remaining roots. C root handles
must not outlive the VM, even if the caller still holds their pointers.
The C++ `Root` wrapper intentionally keeps its VM alive and releases its root
before that VM ownership disappears. Neither wrapper changes the shallow-copy
nature of raw values. Native SSAVM arrays and closure-machine snapshots use
different ownership mechanisms and are not Machinery heap objects.

**References:** [Object.h](../Machinery/Object.h), [GC.h](../Machinery/GC.h), [Memory.h](../Machinery/Memory.h).
