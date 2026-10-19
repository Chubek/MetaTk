# MetaAS implementation report

Date: 2026-10-19

## Delivered

Created `MetaChain/MetaAS`, backed by `/mnt/nvme/metatk/MetaArch`, and added the
MetaChain CMake entry point. MetaAS provides:

- A two-pass assembler library and `metaas` CLI.
- MetaArch `.march`, YAML, JSON, XML and S-expression architecture loading.
- Named encoder arguments and explicit positional mappings, register lookup,
  mnemonic aliases, checked forms, and an explicit `.insn` low-level mode.
- Labels, globals, externals, immutable constants, checked integer expressions,
  sections, alignment, origin, data/string/fill directives, and resource limits.
- Binary/hex output with local absolute data fixups; versioned JSON output with
  sections, symbols, and MetaArch relocation metadata.
- C11-compatible opaque handles, C++20 ownership APIs, static/shared builds,
  installation, documentation, examples and regression tests.

Architecture-specific relocation semantics are preserved for a linker rather
than inferred. Negative symbolic addends are retained and are not mistaken for
final unsigned field values. The README specifies the fixed-layout semantics of
instruction operands that have no declared relocation.

## API additions

`include/MetaAS.hpp`:

- `Result<T>`, `Diagnostic`, `Options`, `Bindings`, `Encoder`.
- `Assembler::load`, `assemble`, `encoders`, `target_count`.
- `Section`, `Symbol`, `Relocation`, `Image`, `Image::binary`, `Image::json`.

`include/MetaAS.h` (ABI version 1):

- `mas_create`, `mas_destroy`, `mas_error`, `mas_load`.
- `mas_set_target`, `mas_set_origin`, `mas_set_limits`, `mas_bind`.
- `mas_assemble`, `mas_image_destroy`, `mas_image_section_count`,
  `mas_image_section`, `mas_image_binary`, `mas_image_json`.

The public headers document ownership and concurrency rules. C entry points
contain exceptions. C++ parse/assembly errors carry diagnostics; allocation
failure may throw. The existing MetaArch APIs and specification data were used
without modifications.

## Verification

Before implementation, the existing MetaArch CTest trees passed:

| Baseline | Tests | Elapsed |
|---|---:|---:|
| MetaArch Release | 7/7 | 13.81 s |
| MetaArch Clang ASan/UBSan | 7/7 | 62.53 s |

Final MetaAS builds used GCC 16.2.1 or Clang 22.1.8, with
`-Wall -Wextra -Wpedantic -Werror` on C and C++ compilation, including the built
MetaArch dependency. All were warning-free.

| Configuration | Tests | Elapsed |
|---|---:|---:|
| GCC Release, MetaChain top-level build | 4/4 | 0.06 s |
| Clang Release, standalone MetaAS | 4/4 | 0.06 s |
| GCC Debug, ASan/UBSan | 4/4 | 0.34 s |
| Clang Debug, ASan/UBSan | 4/4 | 0.24 s |
| GCC Release, shared libraries | 4/4 | 0.06 s |

The core executable contains 86 checks. Additional tests cover the C ABI,
independent JSON parsing with Python, CLI errors/exit codes, output preservation
after failed assembly, and real PPC32 encodings. PPC anchors match MetaArch's
independently checked encoding table: `addi 3,3,-1 = 38 63 ff ff` and
`addi 3,3,1 = 38 63 00 01` in the selected big-endian target.

The shared build was installed to `/tmp/opencode/metaas-install`. The installed
CLI assembled the PPC32 example using the installed architecture database.
A separate C11 compilation linked only `-lmetaas` against the installed public
header/shared library and passed the C ABI tests. Both documented examples ran.

## Reproduce

Run from `/mnt/nvme/metatk/MetaChain`:

```sh
cmake -S . -B /tmp/opencode/metaas-chain \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DFETCHCONTENT_SOURCE_DIR_YYJSON=/mnt/nvme/joltfx/third_party/yyjson
cmake --build /tmp/opencode/metaas-chain -j 6
ctest --test-dir /tmp/opencode/metaas-chain --output-on-failure
```

For standalone builds replace `-S .` with `-S MetaAS`. For Clang set
`-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++` in a new build directory.
For sanitizers additionally use `-DMETAAS_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug`.
For shared libraries set `-DBUILD_SHARED_LIBS=ON`.
The yyjson source override is an environment-specific offline dependency path;
omit it when yyjson is installed or FetchContent can obtain it.

```sh
cmake --install /tmp/opencode/metaas-shared --prefix /tmp/opencode/metaas-install
/tmp/opencode/metaas-install/bin/metaas \
  --spec /tmp/opencode/metaas-install/share/metaarch/ArchDB/ppc32.dsymspec.yaml \
  --format hex MetaAS/examples/ppc32.asm
cc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -I/tmp/opencode/metaas-install/include MetaAS/tests/CAPI.c \
  -L/tmp/opencode/metaas-install/lib -Wl,-rpath,/tmp/opencode/metaas-install/lib \
  -lmetaas -o /tmp/opencode/metaas-installed-c-test
/tmp/opencode/metaas-installed-c-test MetaAS/tests/fixture.march
```

## Current boundaries

- The accepted syntax is encoder-oriented, not GAS/NASM-compatible.
- Ambiguous mnemonic overloads, complex memory syntax, and custom form
  constraints need explicit low-level encoders/parameters.
- JSON is MetaAS's custom interchange format; native object writers, linking,
  branch relaxation, and inferred relocation rules are not implemented.
- `.bss` has materialized bytes; it is not a NOBITS section.
- Local non-relocation instruction operands and `.` use fixed layout addresses.
- ISA coverage/correctness derives from MetaArch's current specification data.
- Verification was on Linux; other platforms were not exercised.

Changes are left in the working tree; no commit or push was performed.
