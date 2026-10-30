# MetaPP

A standalone C preprocessing library with a C11 ABI, a C++17 RAII wrapper,
and the `metacpp` command. No third-party libraries are required.

## Build and run

```sh
cmake -S MetaPP -B build/metapp -DCMAKE_BUILD_TYPE=Release
cmake --build build/metapp -j
ctest --test-dir build/metapp --output-on-failure
build/metapp/metacpp -I include -DDEBUG=1 -UOLD source.c -o source.i
```

Alternatively enable `ARCHDB_BUILD_METAPP` in the parent build. The standalone
build does not configure MetaArch or its dependencies. `METAPP_BUILD_TESTS` and
`METAPP_BUILD_TOOLS` control tests and the executable. Installation exports
`MetaPP::metapp` for `find_package(MetaPP CONFIG REQUIRED)`.

The CLI accepts stdin (`-`, or no filename), `-I`, `-D`, `-U`, `-o`, `-E`, `-P`,
`--help`, `--version`, and `--`. Definitions and undefinitions are applied in
command-line order. Exit status is 0 on success, 1 on preprocessing/I/O failure,
and 2 on usage errors. Diagnostics go to stderr. Output is token-separated C
with comments removed and **no compiler line markers**; original formatting is
not preserved. Input is fully processed before an output file is opened.

## Implemented

* C preprocessing tokens, literals, pp-numbers, trigraphs, digraphs, CRLF
  normalization, backslash-newline splicing and comment replacement.
* Object/function macros, nested argument lists, argument prescan, rescanning,
  per-token hide sets, empty arguments, variadic `__VA_ARGS__`, stringification,
  token pasting and empty-argument placemarkers. Omitted variadic tails are
  accepted as an extension; GNU comma-swallowing is not implemented.
* `#define`, `#undef`, `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`,
  `#include`, `#line`, `#error`, `#pragma`, and the `#warning` extension.
* `defined`, integer/character constants, all C integer arithmetic, comparison,
  bitwise and logical operators, and `?:` in conditional expressions. Logical
  and conditional evaluation short-circuits; unevaluated expressions are still
  parsed. Unknown identifiers evaluate to zero. Integer evaluation uses 64-bit
  signed/unsigned values, usual conversions and arithmetic signed right shift.
  Add/subtract/multiply wrap at 64 bits. Invalid evaluated shifts, division by
  zero, signed division overflow and malformed constants produce errors.
* Quoted includes search the physical including file's directory first, then
  `-I` paths; angle includes search `-I` paths. Include operands may be macros.
  Header guards and canonical-path `#pragma once` work recursively.
* `__FILE__`, `__LINE__`, `__DATE__`, `__TIME__`, `__STDC__`,
  `__STDC_HOSTED__`, and `__STDC_VERSION__` (201112L).
* `_Pragma` with macro-expanded strings. Unknown pragmas are preserved for the
  downstream compiler; `once` is handled internally.

This is an initial implementation, not a claim of complete ISO conformance.
Universal-character-name identifiers are not yet decoded/normalized. Character
constants use byte values (multi-character values pack bytes left-to-right),
not a configurable target execution character set. Compiler-specific macros,
automatic system-header discovery, `#include_next`, `__has_include`, C23
`__VA_OPT__`/`#embed`, dependency-file generation and compiler target emulation
are not supplied. Use explicit `-I` and `-D` settings for a target's headers.
The scaffold's `sizeof`, `offsetof`, `#exec`, and `#eval` are not preprocessing
features in C; those empty placeholders are not compiled.

## Embedding

```cpp
#include <MetaPP.hpp>
metapp::Preprocessor pp;
pp.include_path("include");
pp.define("SQUARE(x) ((x)*(x))");
auto result = pp.preprocess("int n = SQUARE(3);", "example.c");
```

`MetaPP.h` provides the opaque-context C API. It returns 1/0 for success/failure,
contains exceptions at the ABI boundary, and reports diagnostics through
`metapp_diagnostics`. Release returned output with `metapp_free`. A C-only
consumer must link the C++ runtime; the exported CMake target handles this.
The C++ wrapper throws `std::runtime_error` for preprocessing failures.
`metapp_preprocess_n` accepts a pointer and byte count, including buffers that
are not NUL-terminated. It rejects embedded NUL bytes. The C++ wrapper and CLI
use this length-aware path so binary input cannot silently truncate source.

Definitions persist in a context; each preprocessing call resets include-once
state and work counters. A failed call can leave definitions made before the
error in the context. Create a fresh context for each independent translation
unit. Except for the length-delimited source API, strings passed through the C
API are NUL-terminated. Files and stdin reject embedded NULs. Independent contexts can be used concurrently, while operations
on one context must be serialized. Borrowed diagnostics last until the next
operation. Returned outputs are independently owned.

Limits: 64 MiB input per file and output, 64 MiB cumulative macro-substitution
text per preprocessing call, 128 nested includes, 4096 processed files per call
(including the main file), 256 conditional, expression and macro-argument
nesting levels, one million tokens per lex/expansion and two million expansion
steps per preprocessing call. Expansion-byte and file-count counters reset
between calls, including after errors. Allocation failures
become API errors. These limits bound common accidental expansion explosions;
this is not a sandbox for untrusted file access.

## Verification

CTest covers macros, hide sets, stringification, paste, expression semantics,
diagnostics, recursive includes, source locations, C ABI and CLI behavior.
When Python and GCC/Clang are available, it also compares 189 translation units
against the configured C compiler, including the ISO recursive-macro example
and fixed-seed generated conditional expressions. Whitespace is ignored while
preprocessing-token boundaries and literal contents are compared.
Five malformed integer-suffix cases must also be rejected by both tools.
Python-enabled tests exercise 512 fixed-seed byte/macro inputs, binary stdin
and file diagnostics, and preservation of an existing output file on failure.

```sh
cmake -S MetaPP -B build/metapp-san \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build build/metapp-san -j
ctest --test-dir build/metapp-san --output-on-failure
```

Implementation is grouped into `io/lexer.cpp`, `definition/define_macro.cpp`,
`expansion/macro_subst.cpp`, `operations/arithmetic_condition.cpp`, and
`directives/{process,include}.cpp`. `MetaPP.cpp` is the C ABI boundary;
`driver/metacpp.c` exercises that ABI directly.
