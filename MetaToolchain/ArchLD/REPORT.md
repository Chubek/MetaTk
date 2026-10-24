# ArchLD strengthening report — 2026-10-24

## API and compatibility changes

- Added `archld_add_memory(ArchLD *, const char *name, const void *data,
  size_t size)`. It copies ELF objects/ordinary archives and shares the path
  input decoder. Callers retain ownership of their bytes.
- Extended the **standalone** `ArchBFDObjectReloc` with `source_section`,
  `target_section`, `has_addend`. Rebuild consumers of this struct. ELF
  provenance is validated by ArchBFD; COFF uses `SIZE_MAX` for source section.
- Successful post-link symbol/image/map queries are read-only. Mutation and
  destruction require exclusive access; map streams need caller coordination.
- Malformed metadata and malformed/non-ELF/wrong-target archive members now
  fail explicitly, including members that are not needed for symbol resolution.
- Manifest search-directory and library actions now follow encounter order,
  including nested `INCLUDE`. A later `SEARCH_DIR` cannot resolve an earlier
  `LIB`. The documented global fixed-point archive extraction policy remains.
- CMake's `archld` target links separate reader/support targets transitively;
  the existing Make `lib/libarchld.a` remains self-contained.

## Changes

### Strengthened input and diagnostics

The description-driven ArchBFD reader validates ELF identification, version,
header size, section extents/alignment, string tables/references, symbol and
relocation entry sizes, group/relocation links, symbol indices and unsupported
extended numbering/multiple symbol tables. Descriptor field addressing uses
subtraction-based bounds checks; encoded shifts are checked before use.

Archive handling validates GNU name boundaries/terminators, BSD padded names,
empty names, duplicate name tables and padding. Discovery reports bad members
with archive/member context instead of silently skipping them. Relocation
errors retain the input, target section, offset, symbol and underlying reason.

Unsupported writable-executable/executable-NOBITS sections and TLS/IFUNC
definitions are explicitly rejected rather than emitted with misleading
semantics. Final image sizing includes alignment/GOT/COMMON growth.

### Stratified the implementation

Path acquisition, copied memory input and extracted archive input converge on
a shared owned-object decoder/cleanup path. ArchBFD owns relocation provenance;
GOT planning and relocation application no longer reconstruct separate record
streams. Global addresses are finalized before exposing read-only queries.

Standalone CMake separates JSON support, ArchBFD, the CWEB linker and the
DSLtk C++ frontend. It deliberately rejects embedding this satellite ABI into
the full MetaToolchain build. Make now uses workspace DSLtk and tracks headers.
No additional runtime dependencies were introduced.

ArchBFD is the user-confirmed object-format dependency. This integration uses
`ArchLD/third_party/archbfd`; the validation and relocation-provenance changes
are implemented in that standalone library.

### Corrected output and relocation behavior

- Ordinary BSS was previously omitted from `.bss` metadata when its start was
  overwritten after placement. Its start is now retained and aligned.
- Output symbol section indices come from defining sections, avoiding boundary
  ambiguity and preserving BSS/COMMON associations.
- ELF32 RELA sign-extends its explicit addend. Reference:
  [System V GABI §6.1](https://gabi.xinuos.com/elf/06-reloc.html#relocation-entry),
  `Elf32_Sword r_addend`. The regression converts a GNU-assembled REL object
  to equivalent RELA, verifies it with `readelf`, and requires byte-identical
  linked output. No new relocation kinds or HOWTO data were added.
- ELF32 output rejects null/empty paths before writer dispatch. Map-write
  failure paths close their stream.

## Verification

Baseline: both original suites passed, including a Clang ASan/UBSan rebuild;
see `BASELINE.md`. The original Make include path accidentally selected
`/usr/local/include/DSLtk.hpp`. Verification now uses the supplied workspace
header at `../../DSLtk/DSLtk.hpp`.

Final deterministic suite: original ELF and tooling suites, **128 additional
hardening/metadata commands**, C API checks (copied-input ownership, relocation
provenance, invalid writer arguments and four concurrent readers), and a
byte-for-byte CWEB regeneration check. The original tests were retained.

| Build | Compiler | CTest | Elapsed tests |
|---|---|---:|---:|
| Release | GCC 16.2.1 | 4/4 passed | 0.45 s |
| Release | Clang 22.1.8 | 4/4 passed | 0.45 s |
| ASan + UBSan | GCC 16.2.1 | 4/4 passed | 1.64 s |
| ASan + UBSan | Clang 22.1.8 | 4/4 passed | 1.54 s |

All four builds use `-Wall -Wextra -Werror`; sanitizers ran with
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. No warnings or sanitizer
findings occurred in these tests. GNU C11 private `__int128` arithmetic is
retained; this is not a claim of a pedantic ISO-only implementation.
The public `ArchLD.h` independently passed C11 and C++20 syntax checks with
`-Wall -Wextra -Wpedantic -Werror`.

### Corpus testing

Both Clang libFuzzer campaigns completed under ASan/UBSan, with a 16 KiB
input cap, 10-second per-input timeout and 1 GiB RSS limit:

| Target | Duration | Executions | Reported peak RSS | Findings |
|---|---:|---:|---:|---|
| ArchBFD ELF reader | 601 s | 19,832,081 | 479 MiB | None |
| In-memory object/archive linker | 601 s | 78,628 | 433 MiB | None |

Logs are in `build/fuzz-elf.log` and `build/fuzz-link.log`. Generated seed and
retained corpus files are in `build/corpus`, `build/corpus-elf` and
`build/corpus-link`. These campaigns preceded the final ELF32 signed-addend
correction. After rebuilding, the final reader replayed all 189 corpus files
(190 executions including initialization) and the final linker replayed all
186 files (187 executions), with UBSan configured to halt on errors. Both
passed; reported peak RSS was 61 MiB and 405 MiB respectively.

One earlier direct replay reported 2,368 MiB RSS already at corpus startup
and tripped libFuzzer's default 2 GiB limit. The saved input alone passed at
59 MiB; both complete corpora then passed in freshly forked child processes
under the stricter 1 GiB limit. No input-dependent allocation failure was
reproduced. The retained sample is
`build/replay-inputs/oom-0f4926d3795cb9ae0dee3af727ac04519af5911d`; replay logs
are `build/replay-{saved-input,elf,link}.log`.

### Reproduce

Run from `MetaToolchain/ArchLD`:

```sh
make tangle check-generated
make -j4 test

cmake -S . -B build/gcc-release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build/gcc-release -j4
ctest --test-dir build/gcc-release --output-on-failure -j4

cmake -S . -B build/clang-sanitize -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DARCHLD_SANITIZERS=ON
cmake --build build/clang-sanitize -j4
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/clang-sanitize --output-on-failure -j4

make fuzz corpus
mkdir -p build/corpus-elf build/corpus-link
build/fuzz-elf build/corpus-elf build/corpus -max_total_time=600 -max_len=16384 \
  -timeout=10 -rss_limit_mb=1024 -print_final_stats=1
build/fuzz-link build/corpus-link build/corpus -max_total_time=600 -max_len=16384 \
  -timeout=10 -rss_limit_mb=1024 -print_final_stats=1
```

Use separate CMake directories and swap compiler names to reproduce the other
two matrix configurations. Ninja was unavailable on this host; verification
used the Unix Makefiles generator. CWEB remains the source of truth for
`src/ArchLD.c` and `src/ArchLD.h`.

## Remaining work and scope

- Traditional left-to-right archive extraction/group semantics, archive-index
  optimization, section GC, broader GOT/PLT/TLS, executable unwind metadata and
  independent non-ELF output backends remain future work.
- Manifests remain restricted DSLtk manifests, not GNU linker scripts.
- The full Dsymspec runtime and MetaML runtime were not linked or tested here.
- ELF64 fixtures execute on Linux; ELF32 output is structurally validated and
  REL/RELA output compared. COFF/PE inspection has no new fuzzing claim.
- Bounded fuzzing is not exhaustive. Aggregate resource budgeting, overlapping
  input-section handling and larger hostile-input corpora warrant further work.

No architecture descriptions or generated backend databases were modified.
