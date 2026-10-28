# MetaJIT

MetaJIT preprocesses inline MetaIL blocks into C++ that constructs native code.
It uses the real GPPX and MetaIL libraries. Execution currently requires Linux
x86-64 with the System V ABI; preprocessing does not execute generated code.

## Build and verify

From the toolchain root (GPPX, MetaNum, DSLtk, Weighstone, Boost and Lua 5.4 must
be available as required by the respective projects):

```sh
cmake -S MetaJIT -B build-metajit -DCMAKE_BUILD_TYPE=Release
cmake --build build-metajit --parallel 4
ctest --test-dir build-metajit --output-on-failure -j 4
```

`METAJIT_GPPX_ROOT` and `METAJIT_METAIL_ROOT` select source checkouts when installed
packages are unavailable. The root build can enable this project with
`ARCHDB_BUILD_METAJIT=ON`.

See `examples/affine.mjit` for inline blocks and `tests/native.mjit` for supported
operators and control flow. The native backend accepts up to six signed 64-bit
arguments, preserves eager logical operators and lazy `select`, and rejects
unsupported calls, assumptions, undefined locals and fallthrough paths.
Generated numeric literals are independent of the process's numeric locale.

## Lazy whole-method compilation

`MethodBasedJIT.hpp` provides a lazy cache for **trusted machine-code compilers**:

```cpp
metajit::MethodBasedJIT jit;
jit.register_method("answer", [] {
    metajit::CodeBuffer code;
    // Prefix permits instrumented indirect-call probes to read before entry.
    // ENDBR64; mov eax,42; ret (Linux x86-64 only).
    code.append({0,0,0,0,0,0,0,0,0xf3,0x0f,0x1e,0xfa,0xb8,42,0,0,0,0xc3});
    code.entry("answer", 8, 0);
    return code;
});
auto answer = jit.get<std::int64_t (*)()>("answer");
// answer() == 42; subsequent get() calls reuse the published executable.
```

Link `MetaJIT::Runtime`. Registration requires a unique nonempty name without
embedded NULs and a nonempty compiler callback. The callback returns a
`CodeBuffer` containing an entry with that name and the requested arity.
The first `get` compiles, finalizes RW staging bytes into RX memory, validates
the entry/signature, and publishes it. Failures throw without poisoning the
cache and may be retried; recursive compilation of the same method throws
`std::logic_error`. Missing methods, duplicate registrations, and wrong entry
metadata throw `std::invalid_argument`; allocation and OS failures propagate.
`compiled(name)` reports whether an executable has been published.

The cache is noncopyable/nonmovable. Borrowed function pointers remain valid
until cache destruction. Synchronize registration, lookup and destruction
externally; already-published functions can run concurrently while their owner
remains alive. There is deliberately no eviction/invalidation API that could
silently dangle a borrowed native pointer. This cache neither validates machine
code safety nor compiles MetaIL on its own.

## Unimplemented subsystems

`TracingJIT.hpp`, `OSR.hpp`, and `Deopt.hpp` are still empty placeholders, not
working APIs. Implementing them requires decisions absent from the current
runtime: an interpreter/trace recording interface, frame and live-value maps,
safe transition points, and guard/side-exit recovery semantics. The method
cache above does **not** claim to supply these facilities. MetaIL's scope remains
a scalar signed-64 intermediate language, not a tracing runtime.
