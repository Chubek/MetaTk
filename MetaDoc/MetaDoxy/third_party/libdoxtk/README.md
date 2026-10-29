# DoxTk

DoxTk is a C++ document-conversion toolkit organized around intermediate representations (IRs). Its companion `doxir-db/` contains IR schemas described in Doxir, a Lua-based DSL. The intent is to compose readers, parsers, translators, serializers, and writers into reusable conversion tools.

> **Implemented baseline:** dependency-free Plaintext and a documented Markdown subset, AST traversal/validation, UTF-8/ASCII/Latin-1 decoding, deterministic inspection dumps, conversion registry, and file/CLI drivers. This is not a full CommonMark implementation. Other format backends and Doxir execution remain scaffolds.

## Quick start

```sh
cmake -S . -B build -DDOXTK_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The default build should not require any of the optional third-party libraries listed below. See [docs/architecture.md](docs/architecture.md) and [docs/development.md](docs/development.md).

```sh
build/toolchain/doxtk-convert --list-formats
build/toolchain/doxtk-convert --from plaintext --to plaintext input.txt output.txt
build/toolchain/dump2json input.txt output.json
build/toolchain/dump2xml --encoding iso-8859-1 input.txt output.xml
build/toolchain/doxtk-convert --from markdown --to ir-json input.md ast.json
build/toolchain/doxtk-convert --from markdown --to plaintext --allow-lossy input.md output.txt
```

JSON/XML dumps are write-only inspection envelopes, not general-purpose JSON/XML conversion support. Input encoding is explicit (UTF-8 by default); malformed input and unrepresentable XML characters fail without silently replacing content. See the [manual](docs/Frontpage.md) for APIs, capabilities, and limitations.
Markdown-to-Plaintext requires explicit lossiness permission and reports structural loss. See [Markdown subset](docs/manual/markdown.md).

An optional [C++ native DSL](docs/manual/cppdsl.md) builds validated MDAST trees using DSLtk. Enable `DOXTK_ENABLE_CPP_DSL`, configure a DSLtk header directory, and link `DoxTk::CPPDSL`. The default core target remains independent of third-party headers.

## Components
- `include/doxtk/` — public C++ interfaces.
- `src/` — implementation units.
- `doxir-db/` — IR definitions and manifest.
- `toolchain/` — conversion and inspection command-line tools.
- `lua/` and `bindings/` — planned/optional Lua and SWIG integration.
- `tests/` — regression tests and isolated public-header compile checks.

## External dependencies

Optional third-party sources may be present under `third_party/`: `ctl`, `fmt`, `json`, `kaguya`, `klib`, `libglr`, `libharu`, `lua`, `memtkx`, `metatk`, `pugixml`, `sfsexp`, `spdlog`, `xsimd`, `yaml-cpp`. The core does not depend on their presence. Lua, PDF, XML-parser, and YAML integration switches still fail explicitly because those adapters are not wired. The built-in XML dump writer does not require an XML parser library.

## Design principles
- Explicit source and target IRs.
- No silent data loss: unsupported or lossy operations must be surfaced.
- Binary-safe I/O and explicit encoding.
- Small composable library operations; CLI tools remain thin.
- Tests and documentation for every supported format feature.
