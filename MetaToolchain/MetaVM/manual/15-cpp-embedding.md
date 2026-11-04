# Chapter 15 — Embedding through the C++ wrapper

[Previous](14-c-embedding.md) · [Contents](README.md) · [Next](16-host-imports-and-errors.md)

## Select the intended umbrella

`Machinery/MetaVM.hpp` is the C++11 RAII wrapper over the core C API.
The root `MetaVM.hpp` additionally includes C++17 authoring/catalog facilities.
Link the corresponding CMake target so include paths and language requirements
agree. [examples/embed.cpp](examples/embed.cpp) demonstrates a complete wrapper
program; [examples/native.cpp](examples/native.cpp) exercises native profiles.

```cpp
// With only MetaVM::Machinery's include directory: the core C++11 wrapper.
#include "MetaVM.hpp"

auto module = metavm::Module::compile(
    ".function main 0 1\nconst r0 42\nret r0\n.end\n");
metavm::VM vm;
auto result = vm.run(module, module.find("main"));
if (result.kind != MVM_INT) throw std::runtime_error("expected integer");
```

Put such operations inside an application exception boundary. A native-profile
exception is not necessarily `metavm::Error`; catch `std::exception` when the
application wants a common reporting boundary.

## Module interface

`Module` is move-only and uniquely owns its C module. Default construction
creates an empty builder module, not a runnable function. Static `compile`
and `load` return verified owned modules. `find` resolves a function name.
`save` uses the direct-write API; `save_atomic` uses staged replacement.
`get()` provides borrowed C access, with constness following the wrapper.

The raw-pointer ownership constructor is private. To own a C-factory result
without copying it, use an explicit `std::unique_ptr<mvm_module,
decltype(&mvm_module_destroy)>` or another correct application RAII holder.
Do not fabricate a `Module` around an already-owned pointer or arrange two
deleters for the same module.

## VM, results, and roots

`VM(limits)` accepts `mvm_default_limits()` by default. It is move-only.
`run(module, entry, args, engine)` returns a raw tagged `mvm_value`; `get()`
borrows the C VM. `pin(value)` returns a move-only `Root`, whose `value()`
reads the retained value.

```cpp
auto value = vm.run(module);
auto retained = vm.pin(value);
// Later allocations/runs cannot collect retained.value() while retained lives.
```

A `Root` holds shared VM ownership, so it can safely outlive the original
`VM` wrapper. Moving roots preserves correct release order. A raw result does
not gain that lifetime automatically. Copying its `mvm_value` into a vector
still does not root its object.

## Error behavior and host callbacks

`metavm::check(status, error)` throws `metavm::Error` for non-OK statuses.
`Error` derives from `std::runtime_error`, exposes `.status`, and retains a
human-readable message; it does not preserve a complete `mvm_error` structure
as public fields. Keep the C diagnostic separately when exact function/PC/line
coordinates are needed. Allocation can also throw `std::bad_alloc`.

RAII does not permit exceptions across a C callback boundary. Catch them inside
the callback and return an appropriate status/message. Likewise, a wrapper
does not add synchronization, deep-copy values, or make a module safe to mutate
while executing.

**References:** [wrapper source](../Machinery/MetaVM.hpp), [GC ownership](13-objects-and-garbage-collection.md).
