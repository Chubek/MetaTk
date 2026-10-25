# 20. Ownership and error handling

There are three distinct C ownership categories:

1. **Caller descriptors:** your `metail_module` tree and its arrays/strings are
   borrowed during an operation. MetaIL does not free or modify them.
2. **Owned results:** output strings use `metail_free`; parse/optimization module
   snapshots use `metail_module_free`, which frees the entire nested tree.
3. **Borrowed runtime data:** diagnostics and `metail_kernel_info` strings remain
   valid only until the next context operation.

Owned outputs survive context destruction and can be inspected or passed to a
different context. Do not free nested pointers in an owned module individually.
Do not overwrite a result slot before releasing its previous allocation. Output
slots supplied together must be distinct.

`METAIL_INVALID_ARGUMENT` covers missing required top-level inputs/outputs and
invalid round counts. Malformed IR, failed parsing, unknown passes, and runtime
validation failures return `METAIL_RUNTIME_ERROR`. Allocation failures detected
by native allocation paths return `METAIL_OUT_OF_MEMORY`. Always include the
diagnostic when reporting a failed operation rather than relying on status alone.

C++ wrappers manage these allocations with RAII and translate operation failures
to `metail::Error`. Standard container allocations can still throw standard C++
exceptions. Owning C++ objects are kept outside Lua's longjmp boundaries: protected
Lua execution happens in the C bridge.

Partial structured exports are tracked by an allocation list and released if
conversion fails. A failed optimization does not leave a partially rewritten
caller module or invalidate an otherwise usable context.

[Contents](README.md) · [Next: C++ Context](21-cpp-context.md)
