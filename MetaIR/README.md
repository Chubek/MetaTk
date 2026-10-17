# MetaChain

A header-only C++20 toolchain: PEG parsing, term rewriting, a verified integer
IL, a generic virtual machine, a tracing/method JIT, an x86-64 assembler, and an ELF64 static linker.
`MetaChain.hpp` is the collective interface. Its API, text commands, and native
C++ DSL use the sibling [DSLtk](../DSLtk) library.

```cpp
#include "MetaChain.hpp"

int main() {
    using namespace metachain::native;
    Script code;
    auto result = reg(0);
    code.set(result, Expr(6) * Expr(7)).halt(result);
    auto program = code.finish();
    if (!program) return 1;
    auto executed = metachain::Toolchain().run(*program);
    return executed && executed->value == 42 ? 0 : 1;
}
```

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
build/metachain run examples/factorial.mcil
build/metachain build examples/factorial.mcil -o factorial
chmod +x factorial
./factorial
```

The last three commands target Linux x86-64. Parsing, rewriting, IL, and the
interpreter are portable. ELF generation requires a little-endian host. The
assembler takes its registers/opcodes from `Arch/stdspec` and implements a
documented x86-64 subset, not arbitrary Symspec encodings;
the linker is static, non-PIE, and does not replace a system dynamic linker.

See the [complete manual](manual/README.md), particularly the supported-feature
tables and deployment/security boundaries. `Toolchain::jit` runs MetaIL in-process
using tracing, method-based, or hybrid compilation on Linux x86-64, with portable
interpreter fallback. For eager method compilation:

```cpp
metachain::Options options;
options.jit.mode = metachain::jit::Mode::Method;
options.jit.method_threshold = 1;
auto result = metachain::Toolchain(options).jit("const r0, 42\nhalt r0\n");
```

Compiled extensions provide backend selection/scheduling/allocation and IR
algorithms (`MetaChain::Backend`), plus QScript (`MetaChain::QScript`,
`qscript`, `qscript-repl`): a typed scripting language with functions,
constants and 28 embedded libraries, whose parser is a Raku-style scannerless
grammar running on ParseTk. QScript compiles to MetaIL and runs on the
interpreter, the JIT, as a native executable, or through a C99 backend. Grammar
and match programs also run natively: ASMtk emits their instructions and a C++
driver statically links the parser, producing self-contained Linux x86-64 ELF
executables. Simpler native programs retain the small libc-free runtime.
C embedding includes registered callbacks, typed libffi calls, shared-library
loading, binary data exchange, and owning typed host calls for every QScript
value. See
[the QScript manual](manual/qscript/README.md) and the
[backend guide](manual/backend.md).

```sh
build/qscript run qscript/examples/primes.qs
build/qscript --repl
```

Typed FFI requires libffi development files (or configure
`-DQSCRIPT_ENABLE_CFFI=OFF`). Use `-DQSCRIPT_BUILD_SHARED=ON` for a shared
embedding library. See [FFI and C backend examples](manual/qscript/10-ffi.md).

The first [MLVM integer core](manual/mlvm.md) is implemented in QScript:
verified register IR, execution, CFG/dominance/liveness analyses, local/global
constant propagation, interference allocation, serialization, QScript/AMD64
lowering and bounded Arch-generated RV64 leaf code. Other MLVM
subsystems remain planned. Run a sample with
`python3 MLVM/tooling/mlvm.py run MLVM/tests/sum.mlvm`.

The [MetaIR/MLVM bridge](manual/mlvm-bridge.md) (`MLVM::Bridge`) embeds MLVM's
QScript behind a C99 API and exchanges verified integer programs through MCIL.
