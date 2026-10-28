# Juniper

**GPPX macros → Juniper IR → MetaIL passes → instruction selection → scheduling.**

Juniper is a C++20 library and CLI in `Juniper/`. A `Juniperfile` is a program,
not a shell build script: the macro-free form is also its canonical, editable IR.
Plain MetaIL modules are accepted. The real MetaIL runtime owns parsing,
validation, optimization and analysis; Juniper does not substitute an interpreter.

The [36-chapter manual](manual/README.md) includes worked CLI, macro, C++, backend,
MetaArch, and CMake examples, a complete capstone project, and an executable
example checker.

```sh
cmake -S Juniper -B Juniper/build -DCMAKE_BUILD_TYPE=Release
cmake --build Juniper/build -j4
ctest --test-dir Juniper/build --output-on-failure
Juniper/build/juniper emit Juniper/examples/Juniperfile -o example.junir
Juniper/build/juniper optimize example.junir --no-preprocess -o optimized.junir
Juniper/build/juniper schedule optimized.junir --no-preprocess -o machine.json
```

Dependencies: C11/C++20, CMake 3.24, POSIX, and MetaIL's Lua 5.4, Boost, MetaNum,
DSLtk and Weighstone requirements. Python is needed for tests only. GPPX and MetaIL
are found as installed packages first, then built from sibling source checkouts.
No new profile-format library or required runtime dependency is introduced.

From `Juniper/`, equivalent preset builds are `cmake --preset release`,
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
`linkage` (linker manifest), `optimize`, `analyze`, `select`, `schedule`. Selection/scheduling emit deterministic
JSON; `--optimize` runs MetaIL first. Use `--passes` for exact MetaIL kernel names.
With no input path, the input is `Juniperfile`. `--no-preprocess` accepts expanded
IR, including stdin (`-`). `--help` lists all options and exit statuses.

```cmake
add_subdirectory(path/to/Juniper)
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

### libc and shared libraries

```text
@module(foreign)
@extern_symbol(magnitude, (i64), i64, "c", "labs")
@extern_symbol(custom, (i64), i64, "./libcustom.so", "custom_function")
@fn(example, (x))
  @return(custom(magnitude(x)))
@end
```

Plain IR uses `extern fn magnitude(i64) -> i64 from "c" symbol "labs";`.
`symbol` is optional; `@extern(name, (types), result, "library")` uses the same
local/exported name. See [`examples/external.jun`](examples/external.jun).

Imports survive MetaIL optimization and Juniper selection/scheduling. Machine JSON
includes a version-1 `linkage` manifest and an import index on external `CALL`s;
calls remain effect barriers, including libc calls. `juniper linkage file.jun`
extracts the manifest independently; C++ uses `Compiler::linkage(module)`.
`juniper_compile(... MODE linkage)` is also supported. Library names become
`-lNAME` for GNU-compatible drivers; paths containing `/` are single arguments relative to the link working
directory. Append the manifest's `arguments` after objects using an argv API, not
shell interpolation. Search paths, sysroot and runtime loader paths are explicit
driver responsibilities; the compiler does not load or execute shared libraries.

The optional `juniper::encode_external_call` adapter carries the symbol, relocation
addend and imports into a MetaArchIR image with an explicit encoder binding. It
does not invent ABI argument/result moves. MetaArchIR native assembly additionally
supports ELF `.deplibs` autolinking with LLD; other linkers consume the manifest.
ABI signature metadata supports integer widths, pointers, void results and scalar
variadics, **not** floating-point/aggregate lowering. Those signatures need scalar
C wrappers. Juniper remains a selector/scheduler, not a native ABI compiler.

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
