# Contributing to MuLIB

Every new module must have a single self-contained `include/mulib-<name>.h`, a public header guard,
`extern "C"` declarations, a `MULIB_IMPL_<NAME>` implementation switch, a dedicated Doxygen comment
for the file and functions, one detailed `manual/mulib-<name>.md`, and a runnable
`examples/modules/<name>.c` example. Register it in `MANIFEST.yaml`.

The manifest's `module_count` includes reserved filenames;
`implemented_module_count` counts working modules. A `status: reserved` entry is
only for an empty/comment-only future header, must not be a dependency, and must
not be described as an implemented API. Implementing it requires the same
manual, example, switch, and test wiring as any other module.

Implementation switches must be defined in exactly one source translation unit. All public
objects use explicit ownership rules and `mulib_` symbol prefixes. Avoid globals that
cause symbol collisions, document failures and allocation limits, use `size_t` for
capacities, and check multiplication for overflow before allocating. Generic containers
currently copy fixed-stride bytes and are not automatic destructors of nested objects.

Before changing an interface, run `python3 tools/check.py`, the CMake/CTest suite,
strict GCC and Clang compilation if available, and ASan/UBSan functional tests.
Avoid copying third-party sources without recording licensing and provenance.

## Composition and scope

Reuse existing MuLIB storage, parsing, encoding, and document models when they
fit. Include dependency headers normally and list transitive implementation
switches in the manual and standalone example. Never silently define another
module's implementation switch: the application owns implementation placement.

For format and protocol modules, document the exact supported grammar, rejected
features, resource limits, and platform assumptions. A focused API must be fully
implemented; the module name is not a claim of full standards conformance.
Unsupported input should produce a diagnostic rather than silently disappear.
Keep the default library free of third-party dependencies.

Add success, malformed-input, empty-input, boundary, ownership, allocation-failure
(where injectable), and roundtrip tests. New focused suites live in
`tests/test_*.c` and are discovered by CMake and `tools/check.py`. The checker
also compiles headers independently with their transitive implementation
dependencies and validates the manifest/manual/example inventory. CMake's
`MULIB_SANITIZERS` option and the checker's `--sanitizers` flag enable ASan/UBSan.

For shared storage or parser changes, also run `tests/test_hardening.c` (moving
allocation/failure injection) and `tests/test_hostile_inputs.c`. The optional
Clang `MULIB_BUILD_FUZZER` target enables bounded mutation fuzzing; see
`manual/hardening.md` for commands and verification limits. Do not replace
documented focused APIs with claims of full standards or security compliance.
