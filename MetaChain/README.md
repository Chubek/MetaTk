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
assembler implements a documented x86-64 subset, not arbitrary Symspec encodings;
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
`qscript`, `qscript-repl`): an integer scripting language with functions,
constants and 25 embedded libraries, whose parser is a Raku-style scannerless
grammar running on ParseTk. QScript compiles to MetaIL and runs on the
interpreter, the JIT, or as a native executable. See
[the QScript manual](manual/qscript/README.md) and the
[backend guide](manual/backend.md).

```sh
build/qscript run qscript/examples/primes.qs
build/qscript --repl
```
