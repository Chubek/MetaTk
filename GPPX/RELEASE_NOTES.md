# GPPX 1.0.1 — Extension API and driver integration update

This version repairs the previously nearly-empty root headers and the eight extension-category headers from 1.0.0.

## What changed

- Eight `include/GPPX-*.h` category headers now declare useful typed helper APIs and registration helpers.
- `GPPX.c` is no longer a placeholder: it provides compiled C11 utilities for directives, macro CLI specs, syntax profiles, binary-safe readers/writers, byte encoders, literal rewriting and glob/prefix matching.
- The root `GPPX.h` is now an extension-inclusive source-tree umbrella; `include/GPPX-all.h` is its installed equivalent. The root `GPPX.hpp` includes a fluent convenience class around `gppx::Engine`.
- CLI offers `--plugin-dir`, `--check`, `--max-includes`, `--` and improved validation. Configuration supports documented grouped `defines:`, `syntax:` and `options:` entries; plugin loading supports deterministic directory scans.
- REPL offers multiline processing, plugin/config loading, syntax adjustments, matcher inspection and file evaluation.
- Webber assembler now supports additional stack operations, `.EQU`, `.ORG`, `.ALIGN`, `.SPACE`, `.ASCIZ`, string escapes, symbol offsets and forward references with checked errors. A `wbr_hex` embed format was added.
- Built-in and plugin YAML catalogues now document real runtime capabilities. The YAML files are **metadata**, not executable plugin registration or a full YAML schema.
- Added a pure-C extension integration test suite and more end-to-end CLI tests. Both source and install layouts are checked.

## Compatibility and limitations

The native plugin ABI is still version 1: the existing `GPPX_HostAPI` layout and registration signatures have not changed. Category utility additions are additive exported C symbols.

The engine remains a functional, independently designed preprocessing language, **not** a drop-in replacement for a fully standards-conforming ISO C preprocessor. Configuration is a constrained YAML subset, plugins are native unsandboxed code, and Webber is a sample ISA rather than a production assembler for a hardware architecture. See `docs/SECURITY.md` and `docs/EXTENSIONS.md`.

## Verification for this package

- Linux x86-64/GCC debug, release and AddressSanitizer+UndefinedBehaviorSanitizer builds all passed **6/6 CTest jobs**.
- The standalone engine executable reported **61 passing core tests**, including the extended Webber assembler regression.
- The pure C extension test executable reported **47 passing checks**.
- Installed C11 and C++17 clients compiled and executed successfully using the installed headers and shared library.
- Refreshed Linux x86-64 prebuilt executable loaded the refreshed Webber plugin successfully.
- `scripts/verify-install.sh` reproduces the installed-client check.
