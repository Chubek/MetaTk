# MetaVM — versioned scalar register runtime

`vmspec/` implements a dependency-free C++17 register VM and bounded binary
loader/writer. `vmweave/` optionally compiles MetaIL functions using the real
MetaIL parser. The [version-1 specification](vmspec/FORMAT.md) defines the bytecode,
integer/trap rules, host effects, resource limits and ownership contracts.

The VM is portable; [MetaJIT tracing](../MetaJIT/TRACING.md) adds hot-path native
execution, OSR and deoptimization on Linux x86-64. Other hosts retain the same
interpreter semantics. MetaIL itself remains an independent IR library.

## Build

From the toolchain root:

```sh
# Portable core only; no MetaIL, Lua, GPPX, Boost, or MetaNum required.
cmake -S MetaVM -B build-vm -DCMAKE_BUILD_TYPE=Release
cmake --build build-vm --parallel 4
ctest --test-dir build-vm --output-on-failure

# Optional adapter and CLI; uses MetaIL's existing dependency configuration.
cmake -S MetaVM -B build-vm-weave -DCMAKE_BUILD_TYPE=Release -DMETAVM_BUILD_WEAVE=ON
cmake --build build-vm-weave --parallel 4
ctest --test-dir build-vm-weave --output-on-failure -j 4
build-vm-weave/vmweave compile sum MetaVM/examples/sum.mi sum.mvm
build-vm-weave/vmweave run sum.mvm 1000 10
# result=55 steps=...
```

The root build enables the core with `ARCHDB_BUILD_METAVM=ON` (default OFF to
preserve C-only builds). `ARCHDB_BUILD_METAJIT=ON` also enables the VM and, unless
an existing VM target was supplied, its MetaIL adapter. `METAVM_BUILD_TESTS` controls
this project's tests; the optional adapter includes MetaIL's existing tests too.
CLI exits: 0 returned successfully, 1 invalid input/trap/exhausted budget,
2 usage, 3 I/O. The CLI supplies no host callback; use the API for host effects.
The adapter reuses an existing MetaIL target or installed package first, falling
back to the sibling source checkout selected by `METAVM_METAIL_ROOT`.

## API

Link `MetaVM::VM`, include `vmspec/VM.hpp`:

```cpp
metavm::Program add(3, 2, {
    {metavm::Op::Add, 2, 0, 1},
    {metavm::Op::Return, 0, 2}
});
auto frame = metavm::make_frame(add, {20, 22});
auto stop = metavm::run(add, frame, 100);
// stop == metavm::Stop::Returned; frame.result == 42
auto bytes = add.serialize();
auto loaded = metavm::Program::deserialize(bytes);
```

Link `MetaVM::Weave`, include `vmweave/Weave.hpp` for
`weave(source, function, metail_home, limits)`. It returns an owned, validated
Program, rejects possibly uninitialized values and reachable fallthrough, and
preserves left-to-right effects, eager logical operators and lazy `select`.
Calls explicitly enter the host callback namespace; no implicit recursive
function linker or host address lookup is performed. Assumptions are rejected,
not silently ignored, because MetaIL has no executable contract for them.

Programs are immutable and shareable. Frames are mutable and require caller
exclusion; frame/program identity is checked on resume. Host callbacks must not
mutate or destroy active frames/programs. Normal VM traps are statuses, input
errors throw `invalid_argument`/`length_error`, and allocation failures propagate.
These new APIs are C++17; existing C11 toolchain APIs and ABIs are unchanged.
