# 33. CMake integration and dependency tracking

Use `juniper_compile` for standard artifact generation. It invokes the target
executable safely, creates the output directory, and gives CMake a depfile from
the actual macro expansion.

## Consume a source checkout or installed package

An installed-package consumer can use:

```cmake
cmake_minimum_required(VERSION 3.24)
project(GeneratedIR LANGUAGES CXX)
find_package(Juniper CONFIG REQUIRED)

juniper_compile(affine_ir
  INPUT Juniperfile
  OUTPUT artifacts/affine.junir
  MODE emit
  INCLUDE_DIRECTORIES macros
  DEFINES FEATURE=1)
add_custom_target(generate ALL DEPENDS affine_ir)
```

For a source checkout replace `find_package` with an explicit subdirectory:

```cmake
set(JUNIPER_BUILD_TESTS OFF)
set(JUNIPER_BUILD_EXAMPLES OFF)
add_subdirectory(/absolute/path/to/Juniper juniper-dependency)
```

When linking a C++ host, use `target_link_libraries(my_host PRIVATE
Juniper::Juniper)`. For a helper that only generates files, there need not be a
C++ executable in the consumer project.

## The helper's exact contract

```text
juniper_compile(target_name
  INPUT source
  OUTPUT artifact
  [MODE emit|metail|optimize|select|schedule]
  [INCLUDE_DIRECTORIES ...]
  [DEFINES NAME=value ...]
  [DEPENDS ...])
```

`MODE` defaults to `emit`. Relative input/include paths are relative to the
current source directory; relative output paths are relative to the current
binary directory. The function defines a custom target; explicitly build that
target or make another target depend on it. `PREPROCESS`, `PASSES`, `ROUNDS`, and
`OPTIONS` are not accepted helper arguments.

The generated depfile includes the input, prelude, and transitive active includes.
Explicit `DEPENDS` can add application configuration/generator dependencies not
represented by an include. CMake's depfile integration supports the relevant
Make/Ninja generators; use the required CMake version rather than assuming an
old generator implements the same contract.

## Custom pass lists require an explicit command

The following is a **CMake fragment** to add after locating Juniper:

```cmake
set(output "${CMAKE_CURRENT_BINARY_DIR}/special.junir")
add_custom_command(OUTPUT "${output}"
  COMMAND Juniper::juniper optimize "${CMAKE_CURRENT_SOURCE_DIR}/Juniperfile"
    --passes ConstantFolding,AddZeroRight --rounds 2
    -o "${output}" --depfile "${output}.d"
  DEPENDS Juniper::juniper "${CMAKE_CURRENT_SOURCE_DIR}/Juniperfile"
  DEPFILE "${output}.d"
  VERBATIM)
add_custom_target(special_ir DEPENDS "${output}")
```

Use `VERBATIM` and separate arguments rather than assembling a shell command.
Use `--no-preprocess` when the intended input is a frozen canonical artifact and
you do not want to re-enter the macro stage. The convenience helper does not
provide that option; a custom command does.

## Check that dependencies really rebuild

Build once, change an included macro, and build the same artifact target again.
Inspect the new IR, not just the build log. Also test a command-definition change
and changes to the macro standard library. A depfile tracks actual file reads,
not every possible include-search candidate or an unrecorded environment change.
The complete project in chapter 36 demonstrates a multi-stage dependency graph.

[Previous](32-metaarch-encoding.md) · [Index](README.md) · [Next](34-diagnostics-and-security.md)
