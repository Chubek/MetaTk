# MetaArch — Phase B implementation checkpoint

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
