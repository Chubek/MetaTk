# Juniper

**GPPX macros → Juniper IR → MetaIL passes → instruction selection → scheduling.**

Juniper is a C++20 library and CLI in `juniper/`. A `Juniperfile` is a program,
not a shell build script: the macro-free form is also its canonical, editable IR.
Plain MetaIL modules are accepted. The real MetaIL runtime owns parsing,
validation, optimization and analysis; Juniper does not substitute an interpreter.

```sh
cmake -S juniper -B juniper/build -DCMAKE_BUILD_TYPE=Release
cmake --build juniper/build -j4
ctest --test-dir juniper/build --output-on-failure
juniper/build/juniper emit juniper/examples/Juniperfile -o example.junir
juniper/build/juniper optimize example.junir --no-preprocess -o optimized.junir
juniper/build/juniper schedule optimized.junir --no-preprocess -o machine.json
```

Dependencies: C11/C++20, CMake 3.24, POSIX, and MetaIL's Lua 5.4, Boost, MetaNum,
DSLtk and Weighstone requirements. Python is needed for tests only. GPPX and MetaIL
are found as installed packages first, then built from sibling source checkouts.
No new profile-format library or required runtime dependency is introduced.

From `juniper/`, equivalent preset builds are `cmake --preset release`,
`cmake --build --preset release`, and `ctest --preset release`; use the `sanitize`
preset for ASan/UBSan. Optional checked architecture encoding is enabled with
`-DJUNIPER_WITH_METAARCH=ON`.

## Language

```text
@backend(dual_issue)
  @issue(2)
  @resource(alu, 2)
  @rule(add_immediate, add(reg, imm(-2048,2047)), ADDI, 1, 1, alu)
@end
@module(example)
@fn(affine, (x, y))
  @return(add(mul(x, y), 7))
@end
```

`#define @twice mul(#1,2)` defines a macro used as `@twice(x)`.
GPPX supports includes, conditionals and computed macros. Names without `@` do
not expand. See [Language](docs/Language.md) for grammar and precise semantics.

## CLI and build integration

Commands: `preprocess`, `check`, `emit` (complete IR), `metail` (MetaIL only),
`optimize`, `analyze`, `select`, `schedule`. Selection/scheduling emit deterministic
JSON; `--optimize` runs MetaIL first. Use `--passes` for exact MetaIL kernel names.
With no input path, the input is `Juniperfile`. `--no-preprocess` accepts expanded
IR, including stdin (`-`). `--help` lists all options and exit statuses.

```cmake
add_subdirectory(path/to/juniper)
target_link_libraries(my_compiler PRIVATE Juniper::Juniper)
juniper_compile(my_ir INPUT Juniperfile OUTPUT result.junir MODE optimize
                INCLUDE_DIRECTORIES macros DEFINES FEATURE=1)
```

After installation: `find_package(Juniper CONFIG REQUIRED)`. The exported targets
are `Juniper::Juniper` and `Juniper::juniper`; `juniper_compile` works with both
source and installed builds, Make and Ninja, and tracks transitive includes with
depfiles. Outputs cannot alias source dependencies. CLI file writes use temporary
files and atomic replacement, so invalid programs do not truncate prior results.

## Backend configuration and boundaries

Profiles are **C++ values or Juniper backend declarations**, not YAML/JSON/TOML.
`examples/profile.cpp` demonstrates costed fused multiply/add selection. Tree
covering, bounded immediates, resources, latencies, mutable-register hazards,
effect fences, and lazy control flow are implemented, not placeholder passes.
See [Backend](docs/Backend.md) for contracts and the checked MetaArch adapter.

This remains scalar signed-64 IR. It does **not** claim SSA, memory/alias analysis,
register allocation, ABI lowering, object-file emission or a native compiler.
Selected machine operations use virtual registers and explicit control pseudos.
Scheduling is a conservative model, not a processor performance guarantee.

Macro input must be trusted: `#exec` is disabled, no shell constructs are used by
the driver, and expansion has time/output limits, but filesystem reads are not
sandboxed. Runtime discovery can be overridden with `JUNIPER_GPPX`,
`JUNIPER_STDLIB`, `METAIL_HOME`, or the matching CLI options. Compiler contexts
must not be shared concurrently; separate contexts and owned modules are independent.

Build results, scope boundaries and reproduction commands are recorded in
[Verification](docs/Verification.md).
