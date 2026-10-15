# MetaChain

A header-only C++20 toolchain: PEG parsing, term rewriting, a verified integer
IL, a generic virtual machine, an x86-64 assembler, and an ELF64 static linker.
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
tables and deployment/security boundaries. There is no built-in in-process JIT.
