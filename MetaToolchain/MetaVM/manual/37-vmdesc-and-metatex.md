# Chapter 37 — VMDesc and the MetaTeX integration

[Previous](36-dvi-and-replay.md) · [Contents](README.md) · [Next](38-installation-and-sdk.md)

## An embedding, not a second TeX parser

`VMDesc/MetaTeXAdapter.hpp` extends an existing `metatex::Engine` through
`registerCommand`. MetaTeX owns argument reading, expansion, diagnostics,
document construction, formatting, and output devices. VMDesc does not parse
`.vmdesc` as a home-grown TeX dialect or implement its own PDF backend.

```cpp
#include "VMDesc/MetaTeXAdapter.hpp"

metatex::Engine engine;
vmdesc::install(engine);
auto document = engine.compile(source, "machine.vmdesc");
```

This snippet requires the real compatible MetaTeX headers/libraries and its
dependency configuration. It is not part of the dependency-free core. Installing
the adapter header does not automatically find or build those dependencies.

## Registered commands

| Command | Arguments | Document effect |
|---|---|---|
| `\VMName` | One text argument | Set title |
| `\VMArchitecture` | One text argument | Architecture paragraph |
| `\VMStatus` | One text argument | Implementation-status paragraph |
| `\VMInstruction` | Name, operands, semantics | Code span plus semantic text |
| `\VMInvariant` | One text argument | Invariant paragraph |

Handlers have no captured application state. They use
`Engine::readTextArgument`, so nested/expanded argument semantics belong to
MetaTeX, not to a VMDesc string splitter. A rendering integration should validate
those behaviors against the actual engine rather than assume TeX compatibility
solely because command names use backslashes.

Example documentation source:

```tex
\VMName{ToyR16-v1}
\VMArchitecture{16-bit big-endian words and byte registers}
\VMStatus{Explicit example definition; not a historical runtime}
\VMInstruction{load}{x, kk}{Store immediate byte kk in register x.}
\VMInvariant{Unknown instruction encodings are rejected.}
```

Generated catalog `.vmdesc` files deliberately state when they document a
portable example instead of native guest compatibility. Preserve that distinction
in richer rendered documentation.

## Building the optional CLI

Configure `METAVM_ENABLE_VMDESC=ON` when a parent project already provides
`metatex_core`, or set `METAVM_METATEX_SOURCE_DIR` to the real MetaTeX project.
That project must have its own MetaVect/DSLtk/other dependency graph available
as required by its build. Read and retain its own implementation instructions.

```text
vmdesc FILE.vmdesc
```

The current CLI reads a source file, installs commands, compiles it, rejects
MetaTeX error diagnostics, then prints title and document span text. It returns
2 for invalid usage and 1 for an unreadable file/compile failure. It is not a
generic HTML/PDF renderer, and its source-reading path should not be treated
as a hostile-input sandbox with all Machinery resource limits applied.

## Validation status and provenance

The adapter is source-implemented but the optional full MetaTeX integration
and output-device graph remain unvalidated in the recorded MetaVM configurations.
Do not replace missing dependencies with stubs and label that a successful
integration. Genuine validation must compile the supplied engine, run generated
sources, exercise nested arguments/diagnostics, and use its real output devices.

Retain supplied dependency hashes and license/provenance information. A
`.vmdesc` file's existence proves documentation source availability, not native
execution of the named machine.

**References:** [adapter source](../VMDesc/MetaTeXAdapter.hpp), [CLI source](../VMDesc/main.cpp), [dependencies](../third_party/DEPENDENCIES.md).
