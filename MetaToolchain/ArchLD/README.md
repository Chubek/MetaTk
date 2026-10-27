# ArchLD — executable linker for ArchToolchain

**ArchLD is a functioning static linker, not a relocation-only demonstration.**
It accepts real relocatable ELF object files and static `ar` archives, resolves
symbols, places sections, applies architecture-specific relocation rules, and
emits runnable Linux ELF executables. The original ArchBFD parser, metadata
model and object reader provided with the ArchToolchain upload are compiled
into the program.

## Target support

| Target description | Input | Output | Status |
|---|---|---|---|
| `ELF64` / x86-64 little-endian | ELF64 ET_REL, `.o` and `.a` | ELF64 ET_EXEC | Implemented; exercised on Linux x86-64 |
| `ELF32` / i386 little-endian | ELF32 ET_REL, `.o` and `.a` | ELF32 ET_EXEC | Implemented; headers and relocation results checked; kernel execution depends on host's IA-32 compatibility |
| COFF, PE, Mach-O, HPPA, M68K, ECOFF, WASM | ArchBFD has descriptions, but no matching ArchLD backend | — | **Not implemented; explicitly rejected** |

**Scope and readiness:** This is a usable, fully linked static executable
backend for a specified subset of common ELF use cases. It is not yet a
complete replacement for GNU ld, gold, mold, or lld, and should **not** be
represented as universally production-ready for arbitrary binaries. The main
remaining restrictions are documented in `docs/limitations.md`. In particular,
C library startup, TLS, shared objects, linker scripts and all ArchBFD-described
non-x86 targets need additional work. Unrecognized relocations fail rather than
being approximated.

## Build

Requirements: POSIX/Linux x86-64 build host, GCC or Clang with GNU C11
extensions (`__int128`), C++20, `make`, and a C library. DSLtk is header-only
and defaults to the workspace's `../../DSLtk`; override `DSLTK_ROOT` if needed.
Everything needed to parse
ArchBFD's **JSON descriptions** is vendored. No system Dsymspec or YAML
package is required to build or link.

```sh
make -j4
make test
bin/ArchLD --help
```

An isolated CMake build exposes the layers as separate targets:
`archld_json` → `archld_bfd` → `archld` → `ArchTool`. The CMake project is
standalone so the satellite Dsymspec ABI cannot accidentally mix with the
complete MetaToolchain runtime. No external runtime dependencies are added.

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j4
ctest --test-dir build/release --output-on-failure -j4

cmake -S . -B build/asan -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DARCHLD_SANITIZERS=ON
cmake --build build/asan -j4
ctest --test-dir build/asan --output-on-failure -j4
```

Both builds use `-Wall -Wextra -Werror` by default. Make tracks header
dependencies with `.d` files; CMake supports separate compiler/sanitizer trees.
Tests require Python 3, GNU `as`, `ar`, a C compiler and `readelf`/`nm`.

The CWEB literate source is `src/ArchLD.w`; `src/ArchLD.c` and
`src/ArchLD.h` are generated and bundled so users **do not** need CWEB to
build the executable. To regenerate or typeset it:

```sh
make tangle    # requires original Knuth CWEB ctangle
make check-generated  # verifies both bundled outputs in an isolated directory
make docs      # requires cweave and pdftex
```

`docs/ArchLD.pdf` (50 pages) is included. Re-tangle and re-run `make test`
if modifying the CWEB program.

To regenerate the JSON descriptions from the original YAML (regeneration
only; uses Python and PyYAML):

```sh
python3 tools/convert_targets.py
```

The catalogue's original YAML was converted to JSON so ArchBFD can be built
using the bundled `third_party/dsymspec` JSON tree adapter. The generated
ELF64 description additionally includes the GOTPCRELX relocation howtos 41
and 42 absent from the historical YAML data.

## Usage

The source archive can be built in place, or installed with `make install`
(which installs the description catalogue adjacent to `bin/`).

```sh
as --64 -o /tmp/entry.o examples/entry.s
as --64 -o /tmp/run.o examples/run.s

bin/ArchLD -o /tmp/linked /tmp/entry.o /tmp/run.o
/tmp/linked
printf 'exit code: %s\n' "$?"    # 37

ar rcs /tmp/librun.a /tmp/run.o
bin/ArchLD -o /tmp/linked -Map=/tmp/linked.map \
    /tmp/entry.o /tmp/librun.a

readelf -l /tmp/linked
readelf -S /tmp/linked
nm /tmp/linked
```

To select ELF32/i386, use `--target ELF32`. Its default load address is
`0x08049000`; ELF64 defaults to `0x401000`. Override with `--base` (must be
page-aligned). Use `-e` to name another entry point. Use `-L` and `-l` for
static archive discovery. Unsupported command-line options cause errors.

### What linking does

1. ArchBFD loads the selected architecture/format description from its
   versioned catalogue, then parses each ET_REL input and exposes sections,
   symbols and relocation records through its public API.
2. Read and index traditional GNU and BSD `ar` archives. Extract only members
   that satisfy undefined strong symbols, repeating as necessary.
3. Choose a single group for each COMDAT signature, drop duplicate sections,
   and resolve strong/weak/common/absolute/local symbols.
4. Collect allocatable sections (including notes, init/fini arrays, merged
   string sections), page-align segment regions, allocate BSS/common and the
   generated GOT where needed.
5. Resolve each relocation to the chosen symbol address. Apply the supported
   x86-64 `R_X86_64_{64,PC32,PLT32,32,32S,16,PC16,8,PC8,PC64,GOTPCREL,\
   GOTPCRELX,REX_GOTPCRELX}` or i386 `R_386_{32,PC32,PLT32}` howtos.
   REL implicit and RELA explicit addends are supported, with overflow checks.
6. Emit ET_EXEC ELF with non-writable RX text, R rodata, RW data/BSS, a
   non-executable stack program header, symbol/string tables, and section
   headers. Output is committed via an atomic same-directory rename.

The generated GOT is initialized in the executable file, so supported PIC
object references do not need a runtime dynamic linker.

## Library API

`lib/libarchld.a` and `src/ArchLD.h` expose `archld_new`, `archld_add`, `archld_add_memory`,
`archld_link`, `archld_symbol`, `archld_map`, `archld_write_elf` and cleanup.
`archld_define_symbol` installs a copied strong absolute definition;
`archld_set_symbol_resolver` resolves still-missing strong globals after archive
extraction. Successful resolutions are cached. Callbacks must not reenter/free
the context; ELF64 reserves `_GLOBAL_OFFSET_TABLE_` for its own GOT.
When using the standalone archive, only include the bundled `DsymspecIR.h`
and `ArchBFD.h` to avoid conflicting with a separate Dsymspec ABI. The top-level
[MetaLD](../MetaLD/README.md) build integrates this generated core with the full
MetaArch/ArchBFD runtime and supplies a versioned C11 API and C++20 wrapper.
Use `MetaLD::metald` together with `MetaAS::metaas` for same-process assembly
and linking.

`archld_add_memory(ld, name, bytes, size)` copies an object or ordinary archive;
the caller may immediately release its buffer. `name` supplies diagnostic
context. Path and memory inputs share the same decoder and validation path.
Errors are sticky; free a failed instance rather than retrying its phases.
Each instance requires exclusive access while adding, linking or writing.
After successful linking, symbol/image/map queries are read-only and may run
concurrently (use separate streams for concurrent map output). Returned image,
name and error pointers are borrowed until `archld_free`.

The Make archive remains self-contained. CMake consumers link the `archld`
target, which carries its separate ArchBFD and JSON support dependencies.

## Verification

`tests/run.py` builds and links actual assembler and compiler output, executes
ELF64 programs on Linux, validates ELF32 metadata, exercises static archives,
COMDAT, PIC GOTPCRELX, mergeable strings, weak/strong precedence,
unresolved/duplicate-definition errors and deterministic outputs. An additional
sanitizer trial was performed on 120 mutated ELF inputs; this is a smoke test,
not a security audit or a fuzzing-completeness claim.

`tests/hardening.py` adds targeted ELF32/64 corruptions, GNU/BSD archive checks,
readelf-checked BSS/symbol metadata, output preservation on failed links, and
C API buffer-ownership/concurrent-query checks. `tests/check_generated.py`
guards CWEB/generated-source consistency when `ctangle` is available.

Two optional Clang libFuzzer targets cover the standalone ELF reader and the
full in-memory object/archive link path. Run from this directory:

```sh
make fuzz corpus
mkdir -p build/corpus-elf build/corpus-link
build/fuzz-elf build/corpus-elf build/corpus -max_total_time=600 -max_len=16384
build/fuzz-link build/corpus-link build/corpus -max_total_time=600 -max_len=16384
```

See [REPORT.md](REPORT.md) for the measured verification and remaining work.

## Source provenance

`third_party/archbfd/ArchBFD.c` and `.h` are derived directly from the user's
uploaded `ArcnBFD.zip`. The C source has narrowly scoped input-extents
validation changes in its ELF reader to reject malformed headers, symbols and
relocations without unreasonable allocation. See `docs/architecture.md`.

## Command frontends and manifests

This bundle now builds `ArchTool` and symlinked subcommands `ArchLD`, `ArchNm`,
`ArchSize`, `ArchRelocs`, `ArchTargets`, and `ArchLex` instead of the previous
C-only `main.c` executable. The static linker core remains implemented in
CWEB. The C++20 tooling layer is under `src/tooling`; see
`docs/architecture.md` for its dependency boundaries.

An optional `-T file.ald` reads a restricted DSLtk manifest:

```text
SEARCH_DIR(".")
INPUT("entry.o")
LIB(run)
ENTRY(_start)
OUTPUT("linked")
```

Paths in `INPUT`, `SEARCH_DIR`, `OUTPUT` and `INCLUDE` are relative to the
containing manifest. `LIB(name)` searches the directories encountered so far
for `libname.a`. `TARGET(ELF32)` and `BASE(0x08049000)` select output settings.
`INCLUDE("other.ald")` processes directives at that point; include and response
file recursion are bounded. Quotes and `#` comments are supported.

`GROUP(...)` adds inputs to the existing global fixed-point archive search;
it does not implement GNU ld's scoped group or left-to-right extraction
semantics. This is **not** a GNU linker script interpreter.

### Lua integration

The optional `larchld` Lua module exposes the linker library to scripts. Build
with `-DARCHLD_LUA=ON` (Lua is discovered through CMake), then run scripts with
Lua's normal interpreter:

```lua
local ld = require "larchld".new(nil, "ELF64")
ld:add("entry.o")
ld:link()
local entry = assert(ld:symbol("_start"))
ld:write_elf("a.out", entry)
```

The existing `-T` interface remains the LDScript-compatible restricted manifest
mode; Lua is intended for programmable workflows and inspection.
