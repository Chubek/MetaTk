# MetaAS native DSL report — 2026-10-25

API additions: `MetaAS/MetaAS-DSL.hpp`, namespace `metaas::dsl`, in-tree CMake target
`MetaAS::dsl`, and `metaas::Assembler::assembler_argument`. No C ABI changes.

## Changes

The header provides an owning, copyable `Program` with fluent construction and
DSLtk pipeline stages. Typed `Immediate`, `Register`, `Symbol` and `Binding`
values cover explicit encoder inputs and checked forms. It supports sections,
globals, labels, alignment, fixed-width data, arbitrary bytes, source inspection,
and synchronous compilation through the existing MetaAS implementation.

DSLtk supplies `DSL<Program, Pipeline>`, `pipe` and `FixedString` name parameters;
it is used directly from the requested `/mnt/nvme/metatk/DSLtk/DSLtk.hpp` through
`ARCHDB_DSLTK_DIR`. The new header and dependency are installed together. Plain
C/C++ embedding still needs no DSLtk include in consumer code.

Builder validation rejects malformed identifiers, duplicate labels/bindings,
out-of-range immediates/data and invalid alignments. Source and statement limits
use subtraction-based bounds checks. Failed appends preserve state. Moves leave
programs empty and reusable. Integer formatting uses locale-independent
`to_chars`. Strings lower to byte directives so template delimiters and embedded
NULs cannot become source code.

The existing C++ wrapper now rejects embedded NULs in lengthless C string
arguments and diagnostic filenames. Move assignment destroys the previous C
handle before releasing its callback captures. `assembler_argument` exposes the
already-existing C setter without requiring a raw handle.

See README for every builder operation, ownership, error and synchronization
contract, supported output formats, and an executable example.

## Verification

All configurations enabled `-Wall -Wextra -Wpedantic -Werror` with no warnings.
No existing tests were removed or weakened. The new suite contains 64 checks on
this Linux/x86 host plus a compiled example; the test header is used from two
translation units to check linkage. Full suites include all existing database
round trips and embedding/linker tests.

| Configuration | Result | Wall time |
| --- | --- | --- |
| GCC Release baseline | 32/32 | 13.30 s |
| Clang ASan/UBSan baseline | 32/32 | 93.98 s |
| GCC Release after | 34/34 | 13.34 s |
| Clang Release after | 34/34 | 12.48 s |
| GCC Debug ASan/UBSan after | 34/34 | 88.61 s |
| Clang Debug ASan/UBSan after | 34/34 | 96.19 s |
| GCC Release, Lua/plugins disabled; focused tests | 3/3 | 0.08 s |

Sanitizer suites ran outside the ptrace sandbox with leak detection enabled.
Timings are individual runs, not a performance benchmark.

Coverage includes independent expected byte strings for amd64, aarch64 and
riscv64 return instructions; RISC-V addi at -2048, -1, 0, +1 and +2047; invalid
form ranges; relocation symbols/addends; signed and unsigned data boundaries;
source/statement limits and rollback; invalid identifiers; locale-independent
rendering; binary strings; reusable pipelines; callback ownership across move
construction/assignment; moved-from use; result lifetime; and failure/retry.
An optional Linux/x86 GAS test compiles the typed program into an ELF object and
exercises `assembler_argument("--fatal-warnings")`.

Installation was tested into `/tmp/metaas-dsl-install`. Its installed example
compiled with only the installed include tree and linked against installed
MetaAS/ArchAS/MetaArch libraries plus configured system/static dependencies.
It printed `48b82a00000000000000c3` as expected.

## Reproduce

Run from the MetaToolchain root. This machine's yyjson source override avoids a
network fetch; omit it when a system yyjson package is available.

```sh
cmake -S . -B /tmp/metaas-check-gcc -DCMAKE_BUILD_TYPE=Release \
  -DARCHDB_BUILD_METAAS=ON \
  -DARCHDB_DSLTK_DIR=/mnt/nvme/metatk/DSLtk \
  -DFETCHCONTENT_SOURCE_DIR_YYJSON=/mnt/nvme/joltfx/third_party/yyjson \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build /tmp/metaas-check-gcc --parallel 4
ctest --test-dir /tmp/metaas-check-gcc --output-on-failure -j 4
```

Use another build directory with `-DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++` for Clang. For each sanitizer configuration, use
`-DCMAKE_BUILD_TYPE=Debug` and append
`-fsanitize=address,undefined -fno-omit-frame-pointer` to both flag values. In a
ptrace sandbox, run CTest outside it so LeakSanitizer can operate.

For the optional-feature check add `-DARCHAS_LUA=OFF -DARCHAS_PLUGINS=OFF
-DARCHDB_BUILD_METALD=OFF`, build `test_metaas_dsl metaas_dsl_example test_archas`,
and run CTest with `-R 'metaas_dsl|archas_api'`.

```sh
cmake --install /tmp/metaas-check-gcc --prefix /tmp/metaas-install
```

Actual session build trees are `/tmp/metaas-dsl-gcc`, `/tmp/metaas-dsl-clang`,
`/tmp/metaas-dsl-gcc-asan`, `/tmp/metaas-dsl-asan`, and `/tmp/metaas-dsl-minimal`.

## Boundaries

No specification data changed, so there is no encoder-data correction list or
codec output-size change. The underlying encoder/form validation and relocation
contracts remain authoritative. Symbolic form operands remain unsupported by the
underlying API and are rejected explicitly. Native assembly, macros and custom
directive invocations use the existing `Assembler::compile(source)` path.
CMake package exports remain outside this change; `MetaAS::dsl` is an in-tree
target and installed consumers use direct linking, as before. No cross-platform
runtime claim is made beyond the Linux host tested here.
