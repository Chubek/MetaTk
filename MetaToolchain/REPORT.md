# MetaArch — native and cross-language API checkpoint

## Latest addition: ArchAS / domas (2026-10-24)

### API additions

- `ArchAS/ArchAS.hpp`: `archas::Assembler`, `Options`, `Limits`, `Output`,
  `DirectiveContext`, `Directive`, `Syntax`, `Backend`, `Format`, `IOError`;
  `load_target`, `targets`, `has_lua`, `has_plugins`, `read_file`, `write_file`.
  `Assembler::directive` installs a source-producing callback; `compile` returns
  owned output and diagnostics and creates a fresh translation unit per call.
- `ArchAS/ArchASPlugin.h`: C11 plugin ABI v1 with descriptor/version/size checks,
  initialization/shutdown, directive registration, borrowed invocation context,
  bounded source emission and error reporting. Plugins export `archas_plugin_v1`.
- CMake `MetaArch::as`, `archas` library and `domas` tool. New options:
  `ARCHDB_BUILD_ARCHAS`, `ARCHAS_LUA`, `ARCHAS_PLUGINS`, `ARCHAS_LUA_DIR`.
- Existing MetaArch, Dsymspec and MetaArchIR APIs are unchanged.

### Implementation and boundaries

`domas -T amd64 --intel-syntax foo.mair` writes `foo.o`. The frontend expands
GAS macros, repeats, conditions, includes and Lua templates, then lowers explicit
MetaArchIR encoders/forms through the existing validated specification APIs.
Native GAS instructions and general object emission use GNU as or LLVM MC;
target selection is declarative and target endianness comes from Dsymspec.
The builtin backend encodes directly to binary/hex/JSON without a native assembler.
See [ArchAS/README.md](ArchAS/README.md) for language/API/CLI/plugin contracts.

Lua is optional, supports the 5.3/5.4 and 5.5 state-creation APIs, and has
allocation/instruction budgets. CMake prefers system packages, accepts the
provided `third_party/jiterati/third_party/lua` checkout, and has a FetchContent
fallback. Local Lua uses its standard C switch dispatcher to preserve strict C11
compilation. Shared-library plugin loading and native process execution use POSIX.

The native amd64 ELF64 relocation bridge checks Dsymspec metadata against ArchBFD
and emits symbolic `.reloc` records with explicit addends. This follows
[GNU as `.reloc`](https://sourceware.org/binutils/docs/as/Reloc.html); REL in-place
addends are deliberately unsupported. Other targets retain encoded relocations in
JSON or use native assembly instructions. Native flat output uses ArchBFD's ELF
reader, rejects unresolved relocations/multiple payload sections, retains NOBITS
zeros, and omits ELF NOTE metadata. GNU as can synthesize allocated property notes.

All architecture/specification/backend data remains untouched; there are no
encoder data corrections in this task. Existing database coverage/limitations
apply to explicit encoders. This assembly IR has no implicit cross-ISA instruction
translation or register allocation. Alternate GAS macro mode is unsupported.

### Verification notes

The baseline is recorded in `BASELINE.md`. Tests exercise C++/Lua/C plugin
directives, macro recursion/defaults/keywords/variadics, conditional execution,
integer overflow, templates, register metadata, source locations, includes,
output alias protection and optional-extension errors. Native tests inspect
ELF relocations, link and execute the emitted amd64 object with a C caller, and
link a syscall-only entry point using the existing ArchLD executable (exit 37).
LLVM checks cover amd64, AArch64, RISC-V, ppc32 and ppc64le.

One new PowerPC test initially expected big-endian bytes for the shipped ppc64
target. Inspection of `ArchDB/ppc64.dsymspec.yaml:14` established its ppc64le/ELFv2
contract; the fixture now checks little-endian bytes and separately checks
big-endian ppc32. No existing tests were changed or removed.

GNU PowerPC assembler mode/endian flags follow the
[PowerPC options reference](https://sourceware.org/binutils/docs/as/PowerPC_002dOpts.html).
Actual cross-GNU assemblers are not installed; cross-target execution tests use
LLVM MC. Network dependency fetching, non-POSIX hosts, and encoded relocation
object bridges beyond amd64 ELF64 remain unverified or unsupported as documented.

### Final verification

All builds use `-Wall -Wextra -Wpedantic -Werror` for C and C++. No compiler
warnings or sanitizer findings remain. The current ArchAS suite contains **72
API checks and 94 CLI/tool invocations**, including real ArchLD execution.

| Configuration | CTest result | Full suite time |
| --- | --- | --- |
| GCC Release, system Lua 5.5.1 | 27/27 | 12.89 s |
| Clang Release, shared libraries, system Lua 5.5.1 | 27/27 | 14.25 s |
| GCC Debug ASan+UBSan | 27/27 | 89.02 s |
| Clang Debug ASan+UBSan, provided Lua 5.5.0 sources | 27/27 | 100.30 s |
| GCC Release, Lua/plugins disabled (ArchAS tests) | 2/2 | 0.55 s |

These timings are observations, not benchmark claims; some builds ran concurrently.
After the final statement-budget boundary fix and portable plugin-header cleanup,
both ArchAS tests were rebuilt and rerun in all five configurations: GCC Release
0.73 s, Clang shared Release 0.80 s, GCC sanitizers 22.61 s, Clang sanitizers
19.54 s, extensions-disabled 0.55 s. All passed.
The suite grew from 25 to 27 CTest cases. `--emit hex` independently verifies
`ret` as `c3` (amd64), `c0035fd6` (AArch64), and `67800000` (RISC-V).
No codec/output-schema changes were made, so the prior serialization sizes remain
outside this task's scope. The final lint run exits 0; its output is retained in
`build-archas/archas-lint.txt`.

`cmake --install` to `/tmp/opencode/archas-install` succeeded. A separate C++20
consumer compiled against the installed headers/static libraries and encoded
`ret`; the installed `domas` compiled the Intel example using the installed
database. The Lua adapter also passes a strict syntax check against system
Lua 5.4 headers. Runtime tests use system 5.5.1 and the supplied 5.5.0 sources.

Reproduce from the MetaToolchain root (the local yyjson override is only needed
for this offline environment):

```sh
cmake -S . -B build-archas -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DFETCHCONTENT_SOURCE_DIR_YYJSON=/mnt/nvme/joltfx/third_party/yyjson
cmake --build build-archas -j4
ctest --test-dir build-archas --output-on-failure -j4
build-archas/domas -T amd64 --intel-syntax ArchAS/examples/answer.mair -o answer.o
```

For Clang, use a separate tree and add
`-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`; the shared test adds
`-DBUILD_SHARED_LIBS=ON`. For either sanitizer build, use `Debug` and append
`-fsanitize=address,undefined -fno-omit-frame-pointer` to both language flags.
The provided-Lua configuration adds
`-DARCHAS_LUA_DIR=/mnt/nvme/metatk/third_party/jiterati/third_party/lua`.
The minimal configuration adds `-DARCHAS_LUA=OFF -DARCHAS_PLUGINS=OFF` and runs
`ctest --test-dir build-archas-minimal --output-on-failure -R archas`.

Native object writing intentionally remains a GNU/LLVM backend integration;
full standalone ISA parsers/encoders and additional object writers were not
implemented. The earlier MetaArch phase A–G reports below describe prior work,
not additional changes made for ArchAS.

The source snapshot is `build-archas/ArchAS-source.tar.gz`, containing the
committed MetaToolchain tree under the requested `mnt/nvme/metatk/MetaArch/`
archive prefix, including this report. Build products are excluded. Regenerate
from the workspace Git root with:

```sh
git archive --format=tar.gz --prefix=mnt/nvme/metatk/MetaArch/ \
  -o MetaToolchain/build-archas/ArchAS-source.tar.gz HEAD:MetaToolchain
```

## Latest addition: MetaArchIR portable assembly (2026-10-24)

### Follow-up hardening (2026-10-24)

- `metaarch::Spec::encode` now validates encoder lookup before descriptor access,
  checks relocation parameter indices, and handles zero-byte encodings safely.
- `metaarch::Spec::assemble` and `mair::assemble` reject relocation offset
  arithmetic that would wrap `size_t`.
- Focused GCC C++20 syntax checks pass with `-Wall -Wextra -Wpedantic -Werror`
  for `MetaArch.cpp` and the `marchir` CLI. Full CMake/CTest remains blocked in
  this container because Ninja is unavailable and dependency fetching cannot
  resolve GitHub.

### API additions and compatibility changes

- `MetaArchIR/MetaArchIR.hpp` now depends on DSLtk and the MetaArch DSL library;
  consumers link the new `MetaArch::ir` CMake target. The old standalone
  regex-based implementation's single-file compile command no longer applies.
- `mair::Target` is an owning/shared immutable database handle with accessors
  `name`, `index`, `spec`, `description`, `supports`, `forms`, `parameters`, and
  `register_info`. Its old publicly mutable name/database/mnemonic fields were
  removed to prevent constructing apparently valid targets without a loaded spec.
- Added `Location`, `Limits`, typed `Operand`/`Binding`, `SymbolRef`, `reg`, `imm`,
  `symbol`, `Program::encode/form/at/size/empty/clear`, target enumeration,
  `Section`, `Symbol`, `Relocation`, `Image`, `assemble`, and bounded `read_file`.
  Existing builder methods, `parse`, `render`, `target` and `compile_file` remain.
- **Upstream C++ API addition:** `metaarch::Relocation` now retains `bit_offset`,
  `bit_width` and actual `endian` from the C encoding API. This changes the C++
  record layout (rebuild C++ consumers); the C ABI is unchanged. This was necessary
  to preserve mixed-endian relocation fields in portable assembly images.
- `parse` and emission accept source/limit options. Strings now decode escapes;
  numeric directives normalize to decimal and fixed byte widths. Trailing `//`
  comments and whole-line `#` comments avoid swallowing ARM `#` immediates.

### Implementation

The textual frontend uses DSLtk parser combinators and owns its parsed source
data. It reports source file/line/column, checks delimiters/escapes/overflow,
rejects duplicate labels and parameters, and enforces resource limits.

Targets load through `metaarch::Spec::load_dsl`; form and encoder execution go
through the existing public encoding APIs. Named bindings are explicit rather
than inferred from VM parameter occurrence order. There is no high-level IR,
optimization pipeline, analysis pass or register allocation.

`render` supports native assembler text and exact-byte encoded statements.
`assemble` builds section-relative layout, symbols, padding, target-endian data,
and complete relocation field metadata. Symbol/addend references survive in the
image. Flat bytes reject unresolved relocations and multiple sections.

`marchir` has a C entry point backed by the C++ implementation, option manifest,
help/version, target/encoder/form/register discovery, stdin/stdout, validation-only
mode, input/output limits and documented exit codes. It writes assembly, binary,
hex or versioned JSON section images, checks output streams, and refuses to
overwrite the input. Validation errors leave existing output files untouched.

Added six working API examples (including the previously empty Compute/GCD),
five textual examples, native execution checks and LLVM encoding cross-checks.
All eleven API examples are built and run in CTest. The README documents grammar,
API ownership/thread safety, relocation behavior and CLI usage.

### Verification

The original baseline was 12/12 CTest cases; the extended suite contains 25.
The MetaArchIR unit executable passes **114 checks**, covering every one of the
28 database architectures.
Additional CLI tests independently compare AArch64 and RISC-V bytes with
`llvm-mc`/`llvm-objcopy`, execute generated amd64 Compute/GCD code, check JSON
relocations, source diagnostics, budgets and error exit codes.

Final complete runs, all built with `-Wall -Wextra -Wpedantic -Werror`:

| Configuration | CTest | Elapsed |
| --- | --- | --- |
| GCC 16 Release | 25/25 passed | 12.17 s |
| Clang 22 Release | 25/25 passed | 14.00 s |
| GCC Debug ASan+UBSan | 25/25 passed | 81.58 s |
| Clang Debug ASan+UBSan | 25/25 passed | 92.79 s |

No compiler warnings or sanitizer findings in these runs. Configuration emits
the existing yyjson CMake-minimum deprecation warning. The LLVM cross-checks ran
(not skipped). Baseline and final timings are observations, not performance
comparisons: some verification trees ran concurrently.

**Unresolved observation:** during one concurrent multi-build verification,
the unchanged GCC C `archdb_roundtrip` executable segfaulted once. The recorded
core (PID 673214) shows `to_yy -> to_yy -> ... -> dsym_json_write ->
dsym_spec_write -> roundtrip_dir`; `to_yy` received an invalid node pointer.
This executable does not link MetaArchIR or the changed C++ wrapper. An isolated
round-trip rerun passed (12.10 s), followed by the full passing GCC run above;
both sanitizer suites also passed. Root cause was not established, and no
speculative codec patch or test weakening was made. This observation remains
separate from the verified MetaArchIR changes.

Reproduce (Unix Makefiles is used because Ninja is unavailable):

```sh
cmake -S . -B /tmp/opencode/mair-build -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build /tmp/opencode/mair-build --parallel 3
ctest --test-dir /tmp/opencode/mair-build --output-on-failure -j4
```

For Clang use a separate tree and `-DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++`. For ASan/UBSan use Debug and append
`-fsanitize=address,undefined -fno-omit-frame-pointer` to **both** flag variables.
The four verification trees are `mair-build`, `mair-clang`, `mair-asan`, and
`mair-clang-asan`, under `/tmp/opencode`. They share the yyjson source checkout
via `-DFETCHCONTENT_SOURCE_DIR_YYJSON=/tmp/opencode/mair-build/_deps/yyjson-src`.

Installation was exercised with `cmake --install /tmp/opencode/mair-build
--prefix /tmp/opencode/mair-install`; the installed CLI and installed MetaArchDB
produced `1305f5ff67800000` for the RISC-V decrement example.

### Explicit boundaries

Native assembler operands are emitted in the caller's target dialect and only
mnemonic-checked; use checked forms for operand constraints. Low-level encoders
retain their existing VM semantics and database coverage. Hidden/custom form
constraints and unsupported operand kinds still report the underlying API errors.
Symbolic inputs use encoder statements, not checked forms. Relocation expressions
are retained for a linker, not evaluated; no object-file writer or branch
relaxation is introduced. These behaviors are documented and tested, not guessed.

No ISA data was changed. The earlier multi-phase data/schema/codec/fuzzing work
described below remains a separate checkpoint; this task concerns MetaArchIR.

---

## Latest addition: ArchBFD C++ API, XBind specs and language bindings

Extended the ArchBFD subsystem with the surfaces the other parts of the
project have: a C++ API, binding specifications and bindings in the same
languages as the architecture catalogue.

- `ArchBFD.hpp`: a standalone C++17 RAII API over the object-format
  catalogue (`archbfd::api`).  `Catalogue` owns the documents it loads,
  `Spec` owns a document loaded on its own, `SpecView` is the read-only view
  both hand out, and `Object` owns a read object file.  Every string is
  copied into the caller, so it stays valid after the owner is closed.
- `MetaArch.xbind`, `ArchBFD.xbind` and `MetaBFD.xbind`: XBind specs for the
  architecture component, the object-format component and the aggregate.  They
  follow the model of the Tourchstone tree: the full nine-target inventory,
  the query and lifecycle functions, and the out-parameter and buffer APIs
  left out because XBind cannot marshal them.  All three validate, their
  declared C signatures compile against the headers, and the Lua emitter
  lowers them.
- `bindings/d/archbfd.di`: the D interface, including the core diagnostics
  collector.
- `bindings/go/archbfd`: a Go/cgo package with copied values, an explicit
  `Close` and structured diagnostics.
- `bindings/python`: the `metaarch.archbfd` subpackage over the nanobind
  extension, with type stubs.

Two environment problems surfaced and are fixed in the build:

- The Go test inherited a `GOROOT` that does not hold the toolchain, so the
  CTest now computes a usable root (and passes `-vet=off`, which the
  assertions do not need).  The pre-existing Go test benefits as well.
- The D test's argument count was off by one: D's `main` receives the
  program name as `args[0]`.

Verification: the shared build with the foreign and Python bindings enabled
passes 14/14 CTest (the new `archbfd_cpp_api`, `archbfd_d_api`,
`metaarch_xbind_specs` and the extended Go and Python suites), and the
default static build passes 13/13.

## Previous addition: ArchBFD object-format catalogue

Implemented the requested `ArchBFD` subsystem on top of the `binutils/bfd`
checkout in `binutils/` (binutils 2.22, branch `rel_2.22`).  It is a second
catalogue next to `ArchDB`: `ArchDB/` describes instructions, `ArchBFD/`
describes the object formats they are packed into.

- `ArchBFD.h` / `ArchBFD.c`: the description model (`ArchBFDSpec`,
  `ArchBFDRecord`, `ArchBFDReloc`, `ArchBFDConstantGroup`, …), a binder that
  validates a document against the MetaBFD rules with located diagnostics, a
  catalogue (file or directory scan) with lazy loading and container
  inheritance, a writer for the four document formats, and lookup helpers.
- `ArchBFD/*.bfd.yaml`: nine documents — ELF64 (x86-64), ELF32 (i386), the
  HP PA-RISC and m68k ELF emulations, COFF, PE, Mach-O, IRIX/ECOFF and
  WebAssembly.  Every document records the binutils files it was derived from.
- `tools/extract_bfd.py` generates the documents from the checkout by parsing
  the byte layouts (`include/elf/external.h`, `include/coff/*.h`,
  `include/mach-o/external.h`) and the relocation tables (`bfd/elf64-x86-64.c`,
  `bfd/elf32-i386.c`, `bfd/elf32-m68k.c`, `bfd/elf-hppa.h`, `bfd/coff-i386.c`,
  `bfd/coff-mips.c`).  The output is deterministic; `--check` fails when the
  committed documents drift from the checkout.
- `tools/archbfd.c`: `validate`, `convert`, `list`, `catalogue`, `info`,
  `relocs`, `records` and `object`.
- Object reader: `archbfd_object_open()` reads ELF, COFF and PE objects using
  the description alone — the record layouts, the constant groups and the
  relocation table come from the document.
- `ArchBFD/schema/MetaBFD.schema.json`: the document schema.
- Tests: `tests/test_archbfd.c` (3492 checks), `tests/check_bfd_schema.py`
  (schema validation) and `tests/check_bfd_data.py` (cross-check against an
  object produced by the system compiler), all registered with CTest.
- `manual/bfd.md`: the object-format guide.

One defect was found and fixed while writing the reader: an encoding was
applied as `(value & mask) >> shift`, which made the symbol index of an ELF64
relocation always read as zero.  The convention is now `(value >> shift) &
mask`, and the mask always describes the width of the extracted part.  The
generator, the schema and the tests were updated together.

Verification: GCC Release 12/12 CTest (12.5 s) and GCC ASan+UBSan 12/12
(63.8 s), both with `-Wall -Wextra -Wpedantic -Werror` and no findings.  The
pre-existing 1307 architecture checks, 292 form-encoding checks and 289
relocation checks are unchanged.

Known boundaries: the reader supports ELF, COFF and PE; Mach-O, ECOFF and
WebAssembly are descriptions only.  `WAT.bfd.yaml` is the one document that
does not come from binutils — the 2.22 checkout has no WebAssembly BFD back
end, so it follows the WebAssembly core specification, which the file header
states.

## Latest addition: nanobind Python bindings and project manual

Implemented the explicit Python/manual request in `bindings/python` and
`manual` (directory spelling confirmed with the user). No native API/ABI,
schema, spec-data or generated-backend changes were made in this increment.

- Python 3.9+ compiled extension uses nanobind from a configurable checkout or
  installed package; verification used `/mnt/nvme/metatk/third_party/nanobind`
  because `/mnt/nvme/third_party/nanobind` was absent.
- Exposes ABI-v1 load/parse, metadata, lookup, backend loading, four writers,
  named and checked-form encoding, copied results/relocations and diagnostics.
- Context ownership/native cleanup, structured exceptions, strict int64/NUL
  checks, read-only copied metadata and GIL-serialized calls; type stubs included.
- Optional CMake build/test/install and platform wheel support. No separate
  shared core is needed when statically linked. No ctypes/Library/Native API.
- Project manual covers architecture, dependencies, builds, specs/formats,
  CLI, encoder/relocation semantics, native/foreign APIs, Python, DSL, maintenance,
  testing and current limitations; [start here](manual/README.md).

Verification: GCC/Clang Release each pass 6/6 configured CTest cases, including
24 Python tests. Both ASan/UBSan builds pass 5/5 native cases and all 24 Python
tests separately without findings. CMake-installed shared package and installed
platform wheel each pass all Python tests. All four builds are warning-clean
with `-Wall -Wextra -Wpedantic -Werror`. The default DSL-enabled build also
remains green (7/7). Native round trips retain 1307 checks over 28 specs/four
formats; lint output is byte-identical before/after.

Exact commands, timings, packaging details and unverified boundaries are in
[manual/verification.md](manual/verification.md). The larger A–G hardening
roadmap and prior encoder audit remain unchanged; no additional ISA data fix is
claimed by this request. Existing unrelated working-tree changes were preserved.

## Latest addition: C ABI, C++17, D and Go APIs

Implemented the subsequent explicit request for language APIs, without claiming
completion of the remaining original hardening phases.

- `MetaArchABI.h` / `MetaArchABI.c`: opaque **ABI version 1**, with 28 exported
  `metaarch_*` functions. Owned spec/diagnostics/result handles hide native
  layouts. Fixed view types expose copied scalar metadata and borrowed strings.
  Results own relocation names and outlive their originating specs.
- `MetaArchAPI.hpp`: standalone, header-only **C++17** RAII API under
  `metaarch::api`, with move-only specs, owned output/metadata and diagnostic
  exceptions. Coexists with the existing C++20 DSL API and needs no DSLtk.
- `bindings/d/metaarch.di`: D module `metaarch`, with matching C linkage,
  integer widths, view layouts and explicit ownership. All 28 C functions have
  matching declarations; tested with LDC, DMD and GDC.
- `bindings/go`: Go 1.20+ local module `metaarch` using cgo, no third-party Go
  dependencies. Copies outputs/metadata/diagnostics, rejects embedded NUL names,
  and synchronizes Close/backend mutation against reads. Explicit Close required.
- Covers load/parse, backend load, target/register/form summaries, encoder
  lookup, all four writers, named encoding, checked form encoding and complete
  relocation layout. Catalogue and DSL APIs remain native, not wrapped here.
- New APIs are additive: no existing native public structure changed this round.
  The earlier Phase B native ABI change still requires native consumers to
  rebuild; the new opaque ABI avoids that layout coupling for foreign clients.
- CMake installs C/C++ headers, the D interface and Go module sources. Optional
  D/Go tests require shared builds but add no required runtime dependencies.
  Fetched yyjson/libyaml are configured static when MetaArch is shared.

### Binding verification

| Configuration | Result | CTest time |
|---|---|---:|
| GCC Release, native + C/C++ API + existing DSL/CLI | 7/7 pass | 13.35 s |
| Clang Release, same | 7/7 pass | 13.37 s |
| GCC ASan+UBSan, same | 7/7 pass | 66.50 s |
| Clang ASan+UBSan, same | 7/7 pass | 60.17 s |
| GCC shared, DSL off, C/C++/D/Go bindings | 7/7 pass | 14.53 s |
| Clang shared, same | 7/7 pass | 12.92 s |

All C/C++ builds use `-Wall -Wextra -Wpedantic -Werror`, with C++ wrapper tests
explicitly compiled as standard C++17 (no extensions). No sanitizer findings.
The C ABI adds 57 checks to the prior 1888 C checks: **1945 C checks** pass,
plus the C++, D and Go assertion suites. No test weakened or removed. A fresh
C-only build with DSL/C++ tests disabled succeeds without enabling C++.

Additional checks: Go `go test -race` and `GOEXPERIMENT=cgocheck2 go test` pass;
DMD and GDC independently compile/run the same D interface test; C, C++17,
LDC and an external Go consumer compile/run against a staged installation.
`nm -D` confirms every declared `metaarch_*` function is exported (28/28), and
C-header/D-interface function sets match exactly. The synthetic fixture verifies
both target byte orders, all four codec round trips, diagnostics, checked form
ranges, relocation bit metadata and results surviving spec destruction.

The inherited `GOROOT` points at a nonexistent home-directory installation;
Go verification uses `GOROOT=/usr/lib/go` instead. No vet check was disabled.
Tests/builds continue using the local yyjson source override documented below
because network access is unavailable. Installation was staged under
`/tmp/opencode/metaarch-bindings-install` (not a system directory).

Examples, ownership/lifetime rules, supported scope, compiler flags and full
reproduction instructions are in `docs/BINDINGS.md`. To run foreign tests use:

```sh
cmake -S . -B build-bindings -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON -DARCHDB_BUILD_DSL=OFF \
  -DARCHDB_TEST_CPP_API=ON -DARCHDB_TEST_FOREIGN_BINDINGS=ON \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DFETCHCONTENT_SOURCE_DIR_YYJSON=/mnt/nvme/joltfx/third_party/yyjson
cmake --build build-bindings -j8
GOROOT=/usr/lib/go ctest --test-dir build-bindings --output-on-failure
```

Use a valid local GOROOT (or leave it unset) on other hosts. Static/sanitizer
builds run C/C++ ABI tests without foreign runtime integration. The latest
source archive is `/tmp/opencode/MetaArch-bindings.tar.gz` with the original
`mnt/nvme/metatk/MetaArch/` layout, excluding build products. Earlier checkpoint
archives remain available. No architecture or generated backend data changed.

## Prior Phase B checkpoint

## Current API additions / compatibility notes

- Added `dsym_encode_form(const DsymSpec *, size_t form_index, size_t target_index,
  const int64_t *args, size_t n, DsymEncoded *out)`. Visible operands map to
  encoder parameters positionally in first-use order, skipping hidden/implicit
  operands. Checks scalar ranges, signed widths and register class/width/enc.
  Both indices must exist. Count and relocation-kind mismatches are rejected.
- Existing low-level encode APIs remain available and do not enforce form
  constraints. NULL outputs/argument arrays now return failure instead of
  dereferencing NULL.
- Added `bit_offset`, `bit_width` and resolved `endian` to `DsymEncodedReloc`.
  Offset is field-relative, LSB-counted; widths are record widths, not byte-field
  widths. **ABI change:** `DsymEncodedReloc` and `DsymEncoded` grow; rebuild all
  consumers. No existing field was removed. Non-fitting relocation intervals
  now fail at encode time instead of returning unusable metadata.
- New stable resolver lint codes: DSYM-W002 (visible count) and DSYM-W003
  (relocation-parameter kind); strict + lint promotes them to errors.
- `DSYM-W004` checks relocation containment under lint, promoted by strict.
  A narrower relocation in a larger field is valid. Offset/width checks use
  subtraction to avoid overflow on hostile metadata.

Phase B is **in progress**, not finished. No Phase C–G implementation is claimed.
The original Phase A verification and 51-site data audit remain below.

### First Phase B increment verification

Four warning-free `-Wall -Wextra -Wpedantic -Werror` builds pass 4/4 CTest cases:
GCC Release 13.73 s, Clang Release 13.36 s, GCC ASan+UBSan 66.34 s and Clang
ASan+UBSan 60.21 s (single observations with concurrent activity). The focused
`archdb_encode_form` test covers signed/unsigned widths 1/8/63/64/128, explicit
bounds, aliases/class/width, both target byte orders, hidden/implicit operands,
mapping/lint policy, NULL arrays and invalid indices. The existing full test
also checks the real RISC-V addi form and rejects immediates that would otherwise
wrap. No sanitizer findings; no data files changed this round.

New lint reveals 135 count mismatches and 7 relocation-kind mismatches across
existing specs. They are not silently repaired: complex memory decomposition,
unused declared operands and missing relocation statements require individual
reviews. Existing low-level encoding remains available for these forms.

The second increment adds a focused `archdb_relocations` test, covering 26-bit
fields in 32-bit words, nonzero bit offsets, endian-aware linker insertion,
64-bit fields, repeated relocations, field changes, REQUIRE between emission
and relocation, and INT64_MAX offset/width metadata. Invalid relocation spans
are rejected at runtime regardless of load flags. The existing x86 call test
also asserts bit metadata. The ABI and semantics are documented in the header,
README and CHANGELOG.

### Latest verification (both Phase B increments)

| Build | CTest result | Total time |
|---|---|---:|
| GCC Release | 5/5 pass | 13.62 s |
| Clang Release | 5/5 pass | 13.08 s |
| GCC ASan+UBSan | 5/5 pass | 66.14 s |
| Clang ASan+UBSan | 5/5 pass | 59.96 s |

All retain `-Wall -Wextra -Wpedantic -Werror`; no new warning suppression,
dependency, spec-data modification or sanitizer finding. C checks total
**1888**: 1307 full-suite, 292 form-encoding and 289 relocation checks, all
passing (previous checkpoint: 1300 full-suite checks). Optional C++ DSL and CLI
tests pass as well. All 61 independent llvm-mc fixtures still agree. Single-run
timings with concurrent activity are not benchmark regression measurements.

Lint now emits 311 lines: original 112 diagnostics, 39 biased-modulo warnings,
135 mapping-count warnings, 7 mapping-kind warnings and 18 relocation-span
warnings. The 18 span issues remain unresolved in the data:

- ARC: b_s_enc, bl_s_enc, breq_s_enc, brne_s_enc, beq_s_enc, bne_s_enc,
  bgt_s_enc, bge_s_enc, blt_s_enc, ble_s_enc, bhi_s_enc, bhs_s_enc,
  blo_s_enc, bls_s_enc (32-bit relocations on 16-bit fields).
- ARM32: tcbz (32-bit relocation on a 16-bit field).
- AVR: jmp, call (32-bit relocation attaches only to the final 16-bit field,
  not the two-word instruction).
- M68K: jsr (32-bit relocation on a 16-bit field).

These require reviewed data/encoder changes, not broader attachment heuristics.
New runtime behavior intentionally refuses these encodings, avoiding unusable
linker metadata. The remaining Phase B work is encoder built-ins and inferred
intervals, comprehensive diagnostic-code policy/suppression, allocation/OOM
hardening, memory measurements, thread-safety audit and resource limits.

Reproduce with the build commands below plus:

```sh
./build/test_encode_form
./build/test_relocations
ctest --test-dir build --output-on-failure
```

The latest source archive is `/tmp/opencode/MetaArch-phase-b.tar.gz`, using the
same `mnt/nvme/metatk/MetaArch/` root layout and excluding build directories.
The Phase A archive is retained separately. The full seven-phase definition of
done is still not met.

## Historical Phase A checkpoint

## API additions or changes

At the Phase A checkpoint there were **no** API/ABI changes. Strict loading
began rejecting suspicious biased-modulo encoders with diagnostic `DSYM-W001`.
The current Phase B API/ABI additions are listed at the top of this report.

## Status and scope

This is a partial delivery, **not completion of the seven-phase brief**.
Baseline work and the Phase A lint/regression infrastructure are complete.
Twelve data sites are corrected; all remaining 39 are explicitly unresolved.
At that checkpoint Phases B–G had not been started. Phase B is now partially
implemented as described above; Phases C–G remain unstarted.

No existing test was removed or weakened. No generated backend was edited.
No dependency or runtime requirement was added. The source remains C11; the
existing optional C++ DSL is also covered by all four verification builds.

## Changes

- Recorded the original 887-check baseline and original 112-line lint output
  in `BASELINE.md` and `docs/baseline-lint.txt`.
- Identified all 51 biased sites before editing spec data; see the full
  line-numbered inventory and references in `docs/encoder-audit.md`.
- Corrected twelve signed 16-bit wrap expressions without changing opcodes,
  register fields, target byte order or operand contracts.
- Added `tests/encodings.tsv`: five boundary fixtures per corrected encoder
  (-32768, -1, 0, 1, 32767), plus the unchanged RISC-V addi control.
- The C test loads all fixtures. `tests/check_encodings.py` independently
  reassembles every row using llvm-mc; all 61 fixtures agree.
- Added folded-RPN lint detection during encoder resolution immediately after
  compilation. It recognizes PARAM CONST ADD CONST MOD, including embedded
  subexpressions, both ModR/M expressions and REQUIRE expressions.
- Lint emits warnings naming the encoder, statement and source text. Strict
  mode rejects the pattern even without lint. Exact trailing `# biased` opts
  out of this check alone. Tests cover folding, opt-out boundaries, constants,
  safe wrap, zero addend, non-power-of-two modulus and flag combinations.
- Documented every spec change separately in `CHANGELOG.md`.

## All 51 data sites and their disposition

References below are LLVM TableGen files in the local corpus, supplemented by
LLVM 22.1.8 assembler checks. `docs/encoder-audit.md` supplies original line
numbers, detailed field conflicts and reproduction examples.

### Corrected (12)

| Spec | Encoder names | Reference and verification |
|---|---|---|
| ppc32 | mulli, li, addi, addis | PowerPC/PPCInstrInfo.td MULLI, LI, ADDI, ADDIS; PPCInstrFormats.td DForm_base and DForm_2_r0. Five llvm-mc cases each, ppc32 triple. |
| ppc64 | mulli, li, addi, addis | Same definitions. Five llvm-mc cases each, ppc64le triple. |
| mips32 | addi, addiu | Mips/MipsInstrInfo.td ADDi, ADDiu: simm16_relaxed, ADDI_FM. Five llvm-mc cases each, mipsel triple. |
| mips64 | addi, addiu | Same definitions. Five llvm-mc cases each, mips64el triple. |

All twelve now use `(imm + 65536) % 65536`, not the half-modulus bias.
PowerPC instruction word 0x3863FFFF is emitted as `ff ff 63 38` by the ppc64
little-endian spec and `38 63 ff ff` by ppc32. The TSV stores memory-order bytes.

### Unresolved (39), unchanged and still linted

| Spec | Encoder names | Reason / reference |
|---|---|---|
| arc (25) | add_rs12, sub_rs12, mov_rs12, lr_rs12, sr_rs12; ld_rs9, ld_di_rs9, ld_aw_rs9, ldb_rs9, ldb_di_rs9, ldb_aw_rs9, ldh_rs9, ldh_di_rs9, ldh_aw_rs9; st_rs9, st_di_rs9, st_aw_rs9, stb_rs9, stb_di_rs9, stb_aw_rs9, sth_rs9, sth_di_rs9, sth_aw_rs9; ldbx_rs9, ldhx_rs9 | ARCInstrFormats.td immS, F32_DOP_RS12, F32_SOP_RS12, F32_LD_RS9, F32_ST_RS9 show split fields, unlike current contiguous-field multiplications. Register layout/ties also need review. No independent ARC assembler installed. |
| hexagon (2) | r_addi, mpyi | HexagonDepInstrInfo.td A2_addi is signed 16-bit with split bits. M2_mpysmi is pseudo, selecting positive/negative magnitude opcodes (M2_mpysip/M2_mpysin); not a signed 12-bit two's-complement field. llvm-mc rejects immediate mpyi outside -255..255 without extension. Existing opcode/register positions also disagree. |
| loongarch32 (1) | lu12iw | llvm-mc shows immediate shifted by 5 and rd unshifted, opposite current expressions. A modulo-only fix is insufficient. |
| loongarch64 (1) | lu12iw | Same field-placement problem. |
| mips32 (1) | lui | LLVM LUi uses uimm16_relaxed and accepts unsigned 65535 as well as -1. Current forms declare signed range only. Resolve unsigned range versus signed aliases explicitly. |
| mips64 (1) | lui | Same operand-contract conflict. |
| riscv32 (2) | lui, auipc | llvm-mc requires unsigned 20-bit operands, while forms declare signed. Current expressions also omit the field shift by 12. Requires range/contract plus placement correction. |
| riscv64 (2) | lui, auipc | Same conflict and missing shift. |
| s390x (3) | lghi, aghi, chi | llvm-mc shows register nibble at bits 23:20. Current r1*4096 contaminates the immediate; modulo-only correction remains incorrect for nonzero registers. |
| tricore (1) | addi | No local TableGen target or assembler; current immediate overlaps rd*4096. Cannot establish the correct complete encoding confidently. |

These are not intentional-bias exemptions. No unresolved site was annotated
`# biased` merely to suppress its diagnostic. The additional placement defects
need ISA-specific review; this delivery does not claim to fix them.

## Verification results

| Build | Before | After |
|---|---|---|
| GCC Release | 3/3 CTest, 11.90 s | 3/3 CTest, 12.20 s |
| Clang Release | 887 C checks passed | 3/3 CTest, 12.75 s |
| GCC ASan+UBSan | 3/3 CTest, 62.81 s | 3/3 CTest, 64.25 s |
| Clang ASan+UBSan | Not measured | 3/3 CTest, 64.06 s |
| C regression checks | 887, no failures | **1300, no failures** |
| Independent fixture reassembly | None committed | **61/61 agree** |
| Lint | 112 lines, exit 0 | 151 lines, exit 0: same 112 original lines plus 39 DSYM-W001 warnings |
| Strict validation of all specs | Not measured | Exit 1, expected: unresolved biased encoders are rejected |

All four after-builds use `-Wall -Wextra -Wpedantic -Werror` for C and C++ and
build warning-free, retaining the existing library warning configuration.
No sanitizer findings. Timings are single observations, not benchmark medians;
parallel build/test activity makes small timing differences inconclusive.

Output-size and peak-memory benchmarks were **not measured**. No writer changed.
No claim is made about the brief's supplied historical performance estimates.

## Reproduce

This machine lacks Ninja and has unavailable network access. Builds use Unix
Makefiles and local yyjson 0.13.0 via FetchContent source override; the original
CMake fallback requests 0.10.0, which was not available offline. System libyaml
and libxml2 are unchanged. See `BASELINE.md` for original baseline commands.

From the project directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DFETCHCONTENT_SOURCE_DIR_YYJSON=/mnt/nvme/joltfx/third_party/yyjson
cmake --build build -j8
ctest --test-dir build --output-on-failure
./build/test_archdb ArchDB ArchDBCatalogue.yaml
python3 tests/check_encodings.py
./build/archdb validate --lint ArchDB/*.dsymspec.yaml
```

For Clang, use a separate build directory and add
`-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`.
For either sanitizer build, use a separate directory, RelWithDebInfo and append
`-fsanitize=address,undefined -fno-omit-frame-pointer` to both language flags.
Run build and CTest as above. No backend-skipping option was used.
On other machines omit the local yyjson override (install a system package or
allow the existing FetchContent fallback). Ninja may be used if installed.

## Remaining phases — not implemented or verified

| Phase | Outstanding work |
|---|---|
| B | Built-ins, static intervals, comprehensive diagnostic codes/suppression policy, allocation hardening/OOM hook, memory measurements, thread-safety audit and resource limits. Form-aware encode/mapping/ranges and relocation bit semantics are now implemented above. |
| C | Comment-preserving YAML/formatting, source flag order, YAML 1.1 warnings, hybrid JSON, JSON positions, UTF-8/XML correction and XSD, grouped S-expression printing and deterministic golden tests. |
| D | Descriptor reflection/schema sync, output and backend schemas, descriptions, version-dispatch hook. |
| E | Catalogue freshness/duplicates, aliases/triples/ambiguous lookup, lint/diff/fmt CLI, diagnostic formats/exit codes and form/register-friendly encode. |
| F | libFuzzer targets and six ten-minute campaigns, property/cross-parser tests, CI matrix, per-spec CTest splitting, benchmark and regression budgets. No fuzz-duration claims are made. |
| G | CMake/pkg-config/version packaging, symbol visibility/export audit, Doxygen and complete API/development documentation. |

The requested definition of done is **not met**. In particular this is not a
security-hardening release or a claim that all architecture encoders are correct.

## Archive

The original Phase A archive is `/tmp/opencode/MetaArch-phase-a.tar.gz`, rooted at
`mnt/nvme/metatk/MetaArch/`, containing sources, backend data, tests and this report.
Build directories are excluded. The existing optional C++ DSL needs the sibling
DSLtk checkout; for a standalone C-library build use `-DARCHDB_BUILD_DSL=OFF`.
