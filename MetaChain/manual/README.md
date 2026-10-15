# MetaChain manual

This manual describes the implemented MetaChain 1.0 C++20 interface, the
standalone headers, the text languages, and the executable command driver.
It is a practical reference, not a promise that every possible compiler or
architecture feature exists. Unsupported inputs must produce diagnostics.

## Contents

1. [Building, dependencies, and first programs](01-building.md)
2. [Architecture and collective C++ API](02-api.md)
3. [The C++-native DSL and DSLtk](03-native-dsl.md)
4. [MetaIL language and binary specification](04-metail.md)
5. [ParseTk grammars and parsing](05-parsing.md)
6. [ASMtk assembler](06-assembler.md)
7. [LDtk ELF64 static linker](07-linker.md)
8. [Terms, matching, and rewriting](08-rewriting.md)
9. [MetaVM, tracing, and backend contracts](09-vm.md)
10. [Text interfaces and command-line reference](10-text-and-cli.md)
11. [Linux x86-64 native lowering](11-native-backend.md)
12. [Integration recipes](12-recipes.md)
13. [Errors, resource limits, and security](13-safety.md)
14. [Testing, portability, and maintenance](14-testing.md)
15. [Reference tables and migration notes](15-reference.md)

## Choose an entry point

| Goal | Entry point |
|---|---|
| Include everything | `#include "MetaChain.hpp"` |
| Execute IL text | `Toolchain::run(string_view)` |
| Build IL programmatically | `metail::Builder` |
| Build expressions and control flow in C++ | `metachain::native::Script` |
| Parse a custom language | `parsetk::Registry` and `parsetk::Parser` |
| Receive a DSLtk syntax tree | `Toolchain::parse_ast` |
| Normalize first-order terms | `rewrite::System` or `Toolchain::rewrite` |
| Emit x86-64 bytes | `asmtk::Assembler::assemble` |
| Emit an ELF object | `asmtk::Assembler::assemble_object` |
| Link ELF objects | `ld_tk::Linker::link` |
| Compile all MetaIL operations to native code | `Toolchain::executable` |
| Embed text commands | `metachain::evaluate` |

## Capability and platform boundaries

The IL and interpreter implement all 24 documented operations. Native lowering
implements the same integer operations, control flow, stack, output, and halt
semantics using the Linux x86-64 syscall ABI. Native process exit codes expose
only the low eight bits of the IL halt value, unlike the interpreter's full
signed 64-bit result.

ASMtk is an actual encoder for its listed instruction forms. It does not
automatically derive encoders for every architecture described by Symspec.
LDtk accepts conventional little-endian x86-64 ELF relocatables with the listed
RELA relocations. It does not implement archives, dynamic linking, PIE, TLS,
COMDAT, linker scripts, or arbitrary system-toolchain object extensions.

MetaVM has a functional backend protocol for hosts that provide a JIT. MetaChain
does not allocate executable memory or include a built-in JIT compiler. Its
built-in native compiler is ahead-of-time and returns bytes rather than running
them inside the host process.

The library never invokes a shell, external assembler, or external linker to
implement its supported paths. System tools are used only by interoperability
tests and by the user's own deployment workflow.
