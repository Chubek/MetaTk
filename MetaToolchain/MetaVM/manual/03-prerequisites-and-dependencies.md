# Chapter 3 — Prerequisites and dependency policy

[Previous](02-architecture-and-layout.md) · [Contents](README.md) · [Next](04-fallback-build.md)

## Choose the smallest useful configuration

| Configuration | Build requirements |
|---|---|
| Standalone Machinery | ISO C11 compiler; CMake for its supplied build |
| Machinery C++ wrapper tests | C++11 compiler in addition |
| Framework/catalog/native APIs | C11 and C++17 compilers |
| Fallback build | Python 3, C/C++ compilers, `ar` |
| Root CMake default example generation | CMake ≥ 3.18 and Python 3 |
| Actual MetaML adapter | C++20, supplied MetaML, matching DSLtk, threads support |
| VMDesc | C++20 and a real compatible MetaTeX dependency graph |
| Optional dumb-buffer backend | libdrm development files and pkg-config |

Machinery links only the C standard library. The wrapper and native C++ APIs use
the C++ standard library. The core requires 8-bit bytes and exact-width 64-bit
integer types. There is no required LLVM, Lua, JIT, executable-memory facility,
dynamic library loader, or arbitrary-native-ABI FFI.

Python is a build/test dependency for generated examples, not a runtime
dependency of emitted C factories or serialized modules. The root option
`METAVM_GENERATE_EXAMPLES=OFF` omits that generation. Disable tests as well when
you intentionally want a CMake framework build without Python-based checks.

## Matching MetaML and DSLtk

The vendored `third_party/MetaML.hpp` is a supplied dependency with recorded
provenance. Its presence does not make the optional adapter self-contained:
MetaML includes `DSLtk.hpp`. An older DSLtk with a matching filename can still
lack required diagnostic, token, parser, or graph interfaces.

The workspace `DSLtk/DSLtk.hpp` was validated with the supplied header in the
recorded Linux configurations. From the MetaVM directory, that include directory
is `../../DSLtk`. Pass it explicitly rather than relying on accidental global
include paths. Never fabricate stub types to make an incompatible version compile.

MetaML authoring is trusted build-time code. Evaluation limits constrain the
adapter's checkpoints and depth; they do not make imports and host effects an
operating-system sandbox.

## Optional does not mean validated everywhere

VMDesc's source registers commands on `metatex::Engine`; it must be built with
the actual MetaTeX project and its dependencies. The optional libdrm path needs
both a usable build environment and a caller-authorized KMS-capable descriptor
for runtime validation. Core success does not validate either integration.

Linux GCC/Clang configurations, shared-library relocation, and SDK consumers
have recorded tests. Windows/macOS and optional hardware/document-rendering
configurations remain separate validation work.

## Provenance and licensing

Consult `third_party/DEPENDENCIES.md` before enabling or updating a dependency.
It records input hashes and compatible-header observations. CHIP-8 fixtures
have their own upstream revision, full sources, hashes, and license notices.
Do not infer a third-party license grant from inclusion in this repository.
Dependencies and test fixtures must retain their original notices.

**References:** [dependency provenance](../third_party/DEPENDENCIES.md),
[CMake options](05-cmake-builds.md).
