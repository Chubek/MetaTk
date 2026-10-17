# MetaIR ↔ MLVM bridge: QScript and C

The bridge connects MetaIR's implemented integer IL (`metail::Program`) to
MLVM's QScript implementation through the existing **MCIL version 1** wire
format. It does not reinterpret C structs or expose QScript heap handles.

```text
metail::Program ── MetaIR encode ──► MCIL bytes
                                      │
                            MLVM-Bridge.h (C99)
                                      │ owning typed QScript calls
                           metair.qs: import_mcil
                                      ▼
                        MLVM verified instruction rows
                           optimize / analyze / run
                                      │
                           metair.qs: export_mcil
                                      ▼
metail::Program ◄── MetaIR decode ── MCIL bytes
```

`MLVM/source/metair.qs` owns conversion and semantic validation. The other
MLVM QScript libraries own parsing, CFG analyses, optimization and execution.
`MLVM/bridge/Bridge.c` only transports inputs/results and manages ownership via
QScript's typed C API. `MLVM/bridge/Entry.qs` exports the callable entry points.
The C bridge compiles those embedded entry points lazily on its first operation
and caches the resulting program per context. No Python process, shell command,
runtime library search path, external `.qs` files or source interpolation is
involved. All user source/binary inputs are **data**, not QScript code to compile.

## Building and linking

With `METACHAIN_BUILD_QSCRIPT=ON`, CMake builds `MLVMBridge` (`MLVM::Bridge`).
It embeds an explicit list of QScript dependencies as generated C byte arrays;
changes to those sources regenerate the arrays before compilation. Consumers
link `MLVM::Bridge`, which carries its `MetaChain::QScript` dependency:

```cmake
add_executable(my_host host.c)
target_link_libraries(my_host PRIVATE MLVM::Bridge)
```

The public C99 header is `MLVM/include/MLVM-Bridge.h`; it includes only standard
C headers, including when compiled as C++. QScript is implemented in C++, so
manual linking needs a C++ linker driver and QScript's configured dependencies.
No libffi functionality is used by the bridge itself. A QScript build with
`QSCRIPT_ENABLE_CFFI=OFF` can still provide it.

`MLVM/include/MLVM-MetaIR.hpp` is an optional header-only C++20 adapter for
`metail::Program`. It includes MetaIL and the lightweight
`metachain/Outcome.hpp`, not the collective QScript/assembler facade. The
existing `metachain::Stage`, `Diagnostic` and `Outcome<T>` types now live in
that standalone header, with unchanged definitions; `MetaChain.hpp` continues
to include and expose them.

## Shared instruction semantics

The conversion is one-to-one and preserves instruction order, branch targets,
register indices, signed 64-bit immediate bits and MLVM instruction budgets.

| MetaIL | MLVM |
|---|---|
| `Const`, `Mov` | `const`, `mov` |
| `Add`, `Sub`, `Mul`, `Div`, `Mod` | `add`, `sub`, `mul`, `div`, `mod` |
| `And`, `Or`, `Xor`, `Shl`, `Shr` | `and`, `or`, `xor`, `shl`, `shr` |
| `Eq`, `Lt` | `eq`, `lt` |
| `Jmp`, `Jz` | `jmp`, `jz` |
| `Halt` | `ret` |

MLVM `ret` means returning the entry function's integer result. It maps to
MetaIL **`Halt`**, not its stack-based `Ret`. `Nop`, `Jnz`, `Push`, `Pop`,
`Call`, `Ret`, `Print`, `Host` and unknown opcodes are rejected even in
unreachable code. No opcode is silently dropped or emulated with hidden
instructions. Native architecture symbols remain owned by `Arch/`; MCIL
operation numbers here are the existing portable IL wire contract, not a new
machine-opcode bank.

MetaIL permits reads of implicitly zero-initialized registers. MLVM requires
definite assignment on every syntactically reachable path, so such MetaIL
programs are rejected rather than receiving injected definitions. Out-of-range
branches, missing returns/fallthrough past the end, noncanonical unused fields,
malformed headers/reserved bytes, wrong record counts, truncation and trailing
data are also errors. Wrapping arithmetic and trap behavior otherwise match.

MCIL does not carry names, labels, source maps or debug metadata. These are not
preserved; the C++ export returns a verified instruction-only program. Other
MetaIR graph/memory IR forms, QScript compiler-internal host programs, multiple
functions, external bindings and non-integer values are outside this bridge.

## C API and ownership

`MLVM_BRIDGE_ABI` is 1. `mlvm_create()` returns a context or NULL on
allocation/setup failure; `mlvm_destroy(NULL)` is safe. Each context owns its
QScript engine, embedded libraries, cached program and last diagnostic. Contexts
are not concurrent/reentrant; use separate contexts for simultaneous operations.

Operations return `MLVM_OK`, `MLVM_ERROR` or `MLVM_INVALID_ARGUMENT`.
`mlvm_error(context)` borrows the last diagnostic (capped at 4,095 bytes) until
the next operation; success clears it. NULL contexts return an invalid-argument
status and a static diagnostic from `mlvm_error(NULL)`. Failed operations clear
their outputs; they never publish a partially initialized module or buffer.
Caller-owned outputs must be released **before** reusing an output slot.

| Operation | Result |
|---|---|
| `mlvm_parse(c, text, length, &module)` | parse and verify MLVM text |
| `mlvm_import_mcil(c, bytes, length, &module)` | decode and verify MCIL through QScript |
| `mlvm_export_mcil(c, module, &buffer)` | owning MCIL bytes |
| `mlvm_optimize(c, module, mode, &new_module)` | immutable original; owning optimized copy |
| `mlvm_run(c, module, max_steps, &result)` | execute the QScript MLVM interpreter |
| `mlvm_analyze(c, module, &buffer)` | owning UTF-8 JSON analysis report |
| `mlvm_text(c, module, &buffer)` | owning canonical MLVM source |
| `mlvm_module_instruction_count(module)` | count, or 0 for NULL |

Modes are `MLVM_OPT_NONE`, `MLVM_OPT_LOCAL` and `MLVM_OPT_GLOBAL`. Engines are
`MLVM_INTERPRETER` and `MLVM_JIT`, selected with `mlvm_set_engine`; the JIT has
the same off-platform interpreter behavior as QScript. This selects the engine
running the **MLVM implementation**, not a claim that arbitrary imported MCIL
has been compiled directly to native machine code.

`mlvm_module` owns an immutable QScript value snapshot, not a live Session.
Modules and buffers outlive their creation context. Any context can consume a
module; inputs are copied into each fresh QScript call. Release modules with
`mlvm_module_destroy`, buffers with `mlvm_buffer_destroy`; both accept NULL.
Buffer destruction clears its pointer/size. `mlvm_buffer.data` has `size`
payload bytes plus a trailing zero; binary MCIL contains internal zero bytes,
so never use `strlen` on it. Getter data must not be freed separately.

The bridge accepts 1..4,096 instructions (`MLVM_BRIDGE_MAX_INSTRUCTIONS`), at
most 1 MiB of MLVM source (`MLVM_BRIDGE_MAX_SOURCE_BYTES`), and at most
49,164 MCIL bytes. MCIL count/length checks happen before instruction reads.
The C++ importer applies the same instruction ceiling before encoding.
MLVM `max_steps` must be positive and counts MLVM instructions. Separately,
`mlvm_set_host_step_limit` bounds QScript instructions per call (10 million
by default), including verification/analysis work; zero is rejected. Other
QScript heap/AST/stack limits retain their defaults. A host budget trap is not
reported as successful execution. Resource failures leave owning inputs usable.

```c
#include "MLVM/include/MLVM-Bridge.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *source = "module 1 { const r0 42; ret r0; }";
    mlvm_context *c = mlvm_create();
    mlvm_module *m = NULL;
    mlvm_buffer mcil = {NULL, 0};
    int64_t result = 0;
    int status = 1;
    if (!c) return 1;
    if (mlvm_parse(c, source, strlen(source), &m) != MLVM_OK) goto done;
    if (mlvm_run(c, m, 100, &result) != MLVM_OK) goto done;
    if (mlvm_export_mcil(c, m, &mcil) != MLVM_OK) goto done;
    /* Pass mcil.data/mcil.size to MetaIR's metail::decode. */
    status = result == 42 ? 0 : 1;
done:
    if (status) fprintf(stderr, "%s\n", mlvm_error(c));
    mlvm_buffer_destroy(&mcil);
    mlvm_module_destroy(m);
    mlvm_destroy(c);
    return status;
}
```

## C++ MetaIR adapter

`mlvm::metair::import_program(context, program)` returns an
`Outcome<Module>`; `Module` is a `unique_ptr` with the correct C deleter.
`export_program(context, module)` returns `Outcome<metail::Program>`.
MetaIL structural errors are `Stage::Verify` diagnostics with source locations
when available. C bridge/conversion errors are `Stage::Lower` diagnostics.

```cpp
#include "MLVM/include/MLVM-MetaIR.hpp"

int main() {
    auto source = metail::parse("const r0, 42\nhalt r0\n");
    std::unique_ptr<mlvm_context, decltype(&mlvm_destroy)> c(mlvm_create(), mlvm_destroy);
    if (!source || !c) return 1;
    auto module = mlvm::metair::import_program(c.get(), source.program);
    if (!module) return 1;
    auto program = mlvm::metair::export_program(c.get(), module->get());
    if (!program) return 1;
    auto executed = metail::run(*program);
    return executed && executed.value == 42 ? 0 : 1;
}
```

## Validation and checkout limitations

`metachain.mlvm.bridge` builds a pure-C caller plus a C++ fixture using real
MetaIL codecs, verification and execution. It checks interpreter/JIT calls,
every shared arithmetic opcode, bit-exact signed immediates, optimization,
MCIL round trips, invalid arguments, unsupported/noncanonical input, budget
traps, cross-context lifetimes and cleanup. `MLVM/tests/metair.qs` additionally
tests QScript conversion directly; the MLVM runtime tests compile it through
linked C99 and static native backends.

```sh
cmake --build build --target mlvm_bridge_tests
ctest --test-dir build -R 'mlvm.bridge|mlvm.runtime' --output-on-failure
```

The existing missing SAT source and `Arch/stdspec/` dependencies still block a
clean whole-repository build. Bridge C99 code, generated embedded sources and
the MetaIR adapter can be compiled independently and linked against the
pre-existing QScript/backend archives. This path was tested with
`-Wall -Wextra -Wpedantic -Werror` and ASan/UBSan for the new C/adapter code;
the old QScript/backend archives were not rebuilt or sanitizer-instrumented.
Those results do not claim that the blocked full build succeeds.
