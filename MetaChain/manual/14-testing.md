# 14. Testing, portability, and maintenance

## Reproducible builds

GCC build:

```sh
cmake -S . -B build-gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Debug
cmake --build build-gcc -j
ctest --test-dir build-gcc --output-on-failure
```

Clang sanitizer build:

```sh
cmake -S . -B build-clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Debug -DMETACHAIN_SANITIZERS=ON
cmake --build build-clang -j
ctest --test-dir build-clang --output-on-failure
```

Tests use explicit checks rather than `assert`, so Release/NDEBUG builds do not
silently remove the tests. The test executable links multiple translation
units including `MetaChain.hpp`, catching common header-only linkage mistakes.
Each public component header is also compiled in its own generated translation
unit to check self-contained includes.

## Unit coverage

The C++ suite covers:

* PEG sequences, choices, nested repetition, optional forms, keyword boundaries,
  right recursion, left recursion rejection, empty-repeat rejection, grammar
  errors, regex errors, escape decoding, budgets, and transactional events;
* structural term matching, repeated variables, transactional substitutions,
  RHS validation, exact rewrite budgets, growth rules, identity rules, cycles,
  and zero-budget normal forms;
* IL parsing, labels, verification, binary/text round-trips, malformed headers,
  field validation, arithmetic corner cases, stack behavior, and self-loops;
* deterministic randomized optimized/unoptimized execution parity;
* x86 byte encodings, strict strings/comments, forward references, integer
  widths, NUL/output limits, ELF emission, multi-object resolution, and W^X;
* malformed, truncated, and deterministic byte-mutated ELF inputs;
* facade pipelines, native C++ scripts, text parsing/rewriting, lossless
  DSLtk AST/term conversions, depth guards on C++-owned value graphs, and
  configured IL ceilings applied through the facade;
* facade filesystem linking, `IO` versus `Link` diagnostic separation, and
  write-helper failure reporting.

The CLI suite additionally checks argument handling: unknown commands, missing
options, missing input files, unusable input for `link`, and a zero step budget.

## Hostile-input sweep

A deterministic mutation sweep feeds corrupted copies of valid inputs to every
text and binary front end: IL parsing/execution, assembly in both output modes,
term and rule parsing, PEG parsing, MCIL decoding, and the facade's parse, run,
and rewrite entry points. The sweep asserts that each call stays inside its
configured size limits and returns diagnostics instead of crashing.

```sh
build/metachain_tests                       # 2000 mutation rounds
METACHAIN_HOSTILE_SWEEP=1 build/metachain_tests   # 20000 rounds
```

Run it under sanitizers for a much larger budget:

```sh
cmake -S . -B build-asan -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Debug -DMETACHAIN_SANITIZERS=ON
cmake --build build-asan -j
METACHAIN_HOSTILE_SWEEP=1 build-asan/metachain_tests
```

The sweep deliberately uses small step and instruction ceilings. A mutated
branch can otherwise become a legitimate infinite loop that correctly consumes
the default ten-million-step budget, which tests the limit but not the front
ends. Bound the budget when fuzzing for parser/encoder robustness.

These tests are regression checks, not an exhaustive proof. Add minimized
regressions for newly discovered corner cases, particularly malformed binary
arithmetic and event-backtracking behavior.

## Black-box and native tests

When Python is available, `tests/integration.py` exercises the CLI, MCIL binary
round-trips, failures, and generated artifacts. On Linux x86-64 it executes
native images for factorial, integer limits, division/remainder, shifts,
comparisons, calls, stack operations, and runtime error paths.

A deterministic randomized differential program compares all integer operation
outputs between the interpreter and an unoptimized native executable. Disabling
optimization is important: otherwise constant folding could accidentally hide
a broken native arithmetic lowering behind correct precomputed constants.

When GNU tools are available, tests also:

1. link a GNU `as` object with LDtk and run the result;
2. link an ASMtk object with GNU `ld` and run the result;
3. compare supported instruction encodings byte-for-byte with GNU `as`/
   `objcopy`, avoiding forms where the system assembler intentionally chooses
   a shorter encoding.

External tools are optional tests, not production dependencies. Native
execution tests are skipped on incompatible hosts; the portable unit suite
still runs.

## Header-only discipline

New non-template definitions in headers must be `inline`, class members, or
otherwise have safe linkage. Public constants should use inline constexpr
definitions. Do not rely on an include happening to pull a standard header in
transitively; add each header's own dependencies.

Configuration macros must be consistent across translation units, especially
the optional Symspec feature. No generated machine state belongs in a shared
mutable global. A new instruction opcode requires coherent parser, printer,
verifier, codec, interpreter, optimizer policy, native lowering, and tests.

## Adding an assembler form

Document operand widths and exact grammar first. Implement encoding with
explicit range checks and a source diagnostic on unsupported operands. Include
REX, ModRM, SIB, immediate width, displacement sign, and relocation bias tests.
Compare representative encodings with a trusted assembler, including extended
registers and every special addressing base.

Do not accept a Symspec record as proof that its machine encoding is understood.
Metadata and encoding are separate layers. A new architecture deserves a real
backend and a target-specific test matrix, not a permissive fallback.

## Adding an ELF feature

Define the target ABI semantics before accepting new flags/types. Validate
indices, links, ranges, arithmetic overflow, and symbol resolution behavior.
Test both emitted objects and externally produced objects. Check executable
program headers and page permissions with a binary inspection tool.

Rejecting an unsupported feature is safer than recognizing its name while
ignoring its relocation, layout, TLS, property, or dynamic-loader effect.
Changes to MCIL should update its version/decoder policy; changes to the C++
object layout alone must not alter the wire format.

## Portability status

The implementation uses C++20 facilities, including concepts/NTTP features in
DSLtk and `bit_cast`/`span` in MetaChain. The portable components do not require
POSIX. The ELF backend explicitly requires a little-endian host. Generated
executable semantics specifically target Linux x86-64.

The supplied validation exercises GCC and Clang on Linux. Other compilers,
standard libraries, operating systems, optional Symspec configurations, and
cross-endian hosts require their own validation. Do not infer universal support
from successful compilation on one development machine.
