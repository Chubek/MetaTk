# MetaVM implementation and refinement report

Date: November 3, 2026.

## API additions and behavior changes

- `VMLibrary/DVI/DviMachine.hpp`: `DviMachine::run(bytes, metrics)` and
  `run(bytes, metrics, limits)`, owning result/header/font/page/event records,
  independent limits, and a borrowed scaled-font-metric provider.
- `vmweave/DviReplay.hpp`: a stable-address event snapshot/sink owner,
  `module(max_source_bytes)`, `bind(vm, error)` and `document()`. It emits verified
  Machinery host calls, runs under either engine, and translates C++ exceptions
  at the C callback boundary. It does not generate a DVI decoder from Definition IR.
- `dvi-run [--metrics WIDTHS.tsv] [--threaded] FILE.dvi`: installed native DVI
  interpreter/event-replay CLI, with bounded input and explicit missing-metric
  failures. Exit codes and text trace format are documented in `docs/DVI.md`.
- The 12 unmapped legacy example factories now populate `mvm_error`, clear a
  provided output, return `MVM_NOT_FOUND` for an unsupported identity and
  `MVM_INVALID` for a null output pointer. Previously they ignored the error
  object and returned not-found even for a null output pointer. Supported
  factories retain their behavior. All legacy headers document example ownership.
- No C11 Machinery API was changed. Existing host-call failures still become
  `MVM_HOST_ERROR` at the execution boundary, with the callback's detailed message.

## Delivered work and exact coverage

The requested directory is `MetaVM/vmspecs`, containing 38 identities. It held
generated example adapters, not complete guest instruction specifications. The
initial implementation had native subsets for four of these names: FORTH, JVM,
Krivine and SECD. Most example modules return 42 on Machinery. That result does
not establish native compatibility.

This pass adds a real standard DVI format-2 decoder/interpreter. It supports all
defined opcode families, signed operand widths, six positioning registers,
font selection, page/stack transitions, glyphs, visible/invisible rules, opaque
specials, font definitions and pre/post/trailer records. It validates pointer
chains, matching definitions/header units, command placement, input bounds and
resource ceilings. Unknown versions/opcodes and coordinate overflow fail.
Glyph metrics and rendering/special policies are supplied by the host; no
width, filesystem font path or special command execution is guessed.

VMWeave lowers its interpreted events to concrete verified Machinery modules
with a `dvi_event` import. Tests execute both engines and a serialized/deserialized
module. It is event replay, not a reusable generated native decoder.

The full 49-entry catalog now has eight native execution profiles and 41
catalog-only profiles. Among the 38 legacy identities, five have native APIs,
21 are catalog-only and 12 have no catalog mapping. No complete language/platform
runtime is claimed. **Implementing every VM remains unfinished.**

`docs/VMS.md` is a generated manual for every one of the 38 identities, covering
aliases, C entry points, supported native APIs, ownership, current limits and
individual compatibility requirements. `docs/VMS_NATIVE.md` documents the actual
JVM integer and FORTH word subsets; `docs/CLOSURE_VMS.md` retains SECD/Krivine
contracts; `docs/DVI.md` describes the new implementation and reference semantics.
`docs/REFINEMENT.md` records the refinement regimen used for this work.

Generation owns metadata and compatibility adapters. The generator, manifest,
schema and coverage table were updated together. A new CTest/manual check fails
on a missing directory entry or stale manual. Regenerating the catalog produced
byte-identical files and preserved native sources. No required dependency was
added; optional MetaML/MetaTeX/libdrm integrations were not changed or revalidated.

## Semantic references and independent verification

DVI follows Donald E. Knuth, *The DVItype processor*, version 3.6, sections 13–20,
including the instruction table, font definitions and postamble. CTAN distributes
the WEB source at `systems/knuth/dist/texware/dvitype.web` and formatted text at
`info/knuth-pdf/texware/dvitype.pdf`. The implementation is new code; it does not
copy the reference program.

The installed TeX Live 2024 `tex` and `dvitype` were used as independent oracles.
A rule-only document verifies real DVI structure and dimensions. A real `cmr10`
text document obtains scaled advances from DVItype and compares glyph byte
offsets, initial/final horizontal positions and both replay-engine traces.
The core requires neither these programs nor external fonts at runtime.

Native regressions cover every standard opcode family, operand widths, signed
extremes, page/font resets, POP not restoring font, all six position registers,
opaque/NUL specials, invisible-rule advances, malformed prefixes and structures,
duplicate/inconsistent fonts, exact resource limits, coordinate overflow and
4,096 deterministic byte mutations. Replay tests cover serialization, bad event
indices/types, sink exceptions and allocation exceptions. These mutations are not
a claim of sustained fuzzing or exhaustive formal verification.

The refinement caught two incorrect assumptions in newly added tests: Machinery
normalizes callback allocation failures to `MVM_HOST_ERROR`, and NIL uses the
`nil` opcode rather than `const ... nil`. Those tests and the new documentation
were corrected against the existing implementation. No existing semantic test
was deleted or weakened. The catalog-native-count assertion changed from seven
to eight and gained an explicit DVI coverage assertion.

## Baseline and validation

Before edits, the GCC Release build passed all 17 CTest cases in 1.83 seconds;
Machinery retained its 5,056 checks. Ninja was unavailable, so the actual
generator used was Unix Makefiles. Compiler identification was GCC 16.2.1 and
Clang 22.1.8. The final suite has 20 cases: the original 17 plus DVI semantic
regressions, DVI CLI/interoperability, and manual synchronization. Installed SDK
tests compile/run native loading and VMWeave replay after prefix relocation.

Observed final timings are recorded in `docs/BASELINE_VMS.md`. They include
parallel header/SDK tests and vary with concurrent build load; no performance
improvement claim is made. Builds use `-Wall -Wextra -Wpedantic -Werror`.
All 20 cases pass in each of GCC Release (3.61 s), Clang Release (4.16 s),
GCC ASan/UBSan (3.58 s) and Clang ASan/UBSan (4.11 s). Both fallback `make test`
and `make sanitize` also pass, including the independent rule/text oracles.

LeakSanitizer cannot inspect processes in this environment and failed while
running a generated-code build step. Sanitizer verification therefore sets
`ASAN_OPTIONS=detect_leaks=0`, retaining address and undefined-behavior checks;
leak checking is unverified. GCC sanitizer executables use non-PIE linking,
while static libraries use PIC so installed consumers can link correctly.
An initial non-PIC sanitizer configuration failed that SDK test and was replaced
with the documented configuration. Occasional concurrent Make FIFO messages
were orchestration messages, not compiler diagnostics.

## Reproduce

Run from the repository root. The default CMake warning policy already enables
all four warning flags above.

```sh
cmake -S MetaVM -B /tmp/metavm-vms-baseline -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build /tmp/metavm-vms-baseline -j4
ctest --test-dir /tmp/metavm-vms-baseline --output-on-failure -j4

cmake -S MetaVM -B /tmp/metavm-vms-clang-release -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build /tmp/metavm-vms-clang-release -j4
ctest --test-dir /tmp/metavm-vms-clang-release --output-on-failure -j4

cmake -S MetaVM -B /tmp/metavm-vms-clang-san -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug -DMETAVM_SANITIZE=ON -DMETAVM_TEST_CPP=ON \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
ASAN_OPTIONS=detect_leaks=0 cmake --build /tmp/metavm-vms-clang-san -j4
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir /tmp/metavm-vms-clang-san --output-on-failure -j4

cmake -S MetaVM -B /tmp/metavm-vms-gcc-san -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug -DMETAVM_SANITIZE=ON -DMETAVM_TEST_CPP=ON \
  -DCMAKE_C_FLAGS=-fPIC -DCMAKE_CXX_FLAGS=-fPIC -DCMAKE_EXE_LINKER_FLAGS=-no-pie
ASAN_OPTIONS=detect_leaks=0 cmake --build /tmp/metavm-vms-gcc-san -j4
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 LDFLAGS=-no-pie \
  ctest --test-dir /tmp/metavm-vms-gcc-san --output-on-failure -j4

make -C MetaVM test
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 make -C MetaVM sanitize
python3 MetaVM/tools/validate_catalog.py
python3 MetaVM/tools/generate_vmspec_manual.py --check
python3 MetaVM/tools/test_dvi.py /tmp/metavm-vms-baseline/dvi-run
```

`LDFLAGS=-no-pie` also configures the GCC sanitizer SDK test's external consumer
builds. Normal Release consumers need no such setting. Regenerate documentation
with `python3 MetaVM/tools/generate_vmspec_manual.py`; generate catalog-owned
files with `python3 MetaVM/tools/generate_catalog.py`.

## Remaining implementation work

Every unmet target is listed separately in `docs/VMS.md` and
`tools/legacy_profiles.py`. The 33 identities without native execution are ALEX,
BCPL, BEAM, BMachine, C-Machine, CAM, CESK, CLR, CPython, FAM, FUL, GMachine,
GraalVM, Harrison, LASSIE, LuaJIT, LuaVM, P-Code, Parrot, Postscript, Prol, PyPy,
SOM, Smalltalk-80, SynthBE, TAM, Truffles, VMGL, VirGL, WAM, WASM, YARV and Zend.
They need actual implementation as well as resolved version/profile contracts;
the manual is not a substitute. Several public runtimes have published but
version-dependent instruction sets. Other names have no supplied authoritative
machine definition. No instruction semantics were inferred merely from a name.

The existing JVM/FORTH/SECD/Krivine implementations remain subsets with their
previous documented omissions. DVI needs external scaled metrics, font/rendering
services and any desired driver-special behavior; it has no TFM loader,
rasterizer, XDV or pTeX extensions. Optional integration validation, sustained
fuzzing, leak checking and non-Linux builds remain unverified in this pass.
