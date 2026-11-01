# mulib-wask

A small, embeddable **i32 WebAssembly binary module runtime**. This is a focused
straight-line execution profile, not a complete WebAssembly engine or WASI host.
No third-party library, native code generation, filesystem or network access.

Define `MULIB_IMPL_WASK`, `MULIB_IMPL_DSYMSPEC`, `MULIB_IMPL_JSON`,
`MULIB_IMPL_PARSEC`, `MULIB_IMPL_AST`, `MULIB_IMPL_STRBUF`, and `MULIB_IMPL_UTF8`
in exactly one C11 translation unit before including `mulib-wask.h`. No switch
is enabled implicitly. Declarations have C++ linkage guards.

`mulib_wask_load(bytes, length, &error)` validates and copies the binary. Destroy
the returned owned module with `mulib_wask_destroy`; NULL is harmless. Source
bytes may be released immediately. A failed load releases partial allocations.
Call an exported function with `mulib_wask_call(module, name, arguments, count,
fuel, &result, &error)`. Arguments/results are unsigned i32 bit patterns; signed
interpretation is instruction-specific. Result changes only on success; void
functions produce zero. Modules are immutable, so calls may run concurrently;
destroy only after all calls have finished. No host callbacks or shared globals.

The loader reads the builtin `wasm` architecture through `mulib-dsymspec.h`, finds
each supported mnemonic's form and encoder, and takes the first `EMIT8` opcode
from its metadata. It does not execute encoder pseudo-code or use its modeled
fixed-width immediates: the runtime decodes actual bounded u32/s32 LEB128.
Metadata is released after the per-module dispatch map is built.

Supported binary format: magic/version 1; ordered, unique type (1), function
(3), export (7), code (10) sections; arbitrary custom (0) sections with valid
UTF-8 names. Empty modules are valid. Function signatures have i32 parameters
and zero or one i32 result. Locals are zero initialized. Function export names
are unique UTF-8 byte strings; the call API addresses names without embedded
NULs. Custom payloads are skipped after name validation.

Supported instructions: `nop`, terminal `end`, `drop`, `select`, `local.get`,
`local.set`, `local.tee`, `i32.const`, `i32.add`, `i32.sub`, `i32.mul`,
`i32.div_s`, `i32.and`, `i32.or`, `i32.xor`, `i32.eqz`. Addition, subtraction
and multiplication wrap at 32 bits. Signed division truncates toward zero and
traps for zero divisor or INT32_MIN / -1. Every function is decoded and its
stack/local accesses checked during loading, even if never called.

All other instructions/sections produce `MULIB_WASK_UNSUPPORTED`, including
imports, memory, globals, tables, calls, structured control flow, return,
unreachable (polymorphic dead code), floats, i64, SIMD and WASI. No unsupported
feature is silently ignored. Resource limits may take precedence over an
unsupported/invalid diagnosis for oversized records.

Hard limits: 1 MiB module; 128 types/functions/exports each; 256 parameters plus
locals per function; 256 local declaration groups; 256 operand stack entries.
These bound load-time work. Calls additionally require caller-provided fuel;
each instruction, including `end`, consumes one unit. Fuel exhaustion is
`MULIB_WASK_LIMIT`; zero fuel executes nothing. Other statuses distinguish
invalid bytes/arguments, allocation failure, arithmetic traps and missing
exports. Error messages are static; offsets are decoder positions in the module
(after the offending byte/immediate in many cases), not instruction indices.
An optional error pointer can be NULL.

See `examples/modules/wask.c` and `tests/test_wask.c` for execution, ownership,
wrapping arithmetic, traps, truncation, malformed LEBs and bounded mutations.

`mulib_wask_version()` returns API revision 1.
`mulib_wask_supports(mnemonic)` reports exact instruction support without loading a module.
