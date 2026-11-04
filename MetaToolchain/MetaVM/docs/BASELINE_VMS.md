# MetaVM baseline and refinement validation

Observed November 3, 2026, on the supplied Linux workspace. GCC identifies as
16.2.1 and Clang as 22.1.8. Times below are CTest's reported real times with
`-j4`. The four final suites ran concurrently, so these are observations rather
than controlled performance comparisons.

| Configuration | Before | After | Final result |
|---|---|---|---|
| GCC Release | 17 tests / 1.83 s | 20 tests / 3.61 s | Pass |
| Clang Release | Not measured before edits | 20 tests / 4.16 s | Pass |
| GCC ASan + UBSan | Not measured before edits | 20 tests / 3.58 s | Pass |
| Clang ASan + UBSan | Not measured before edits | 20 tests / 4.11 s | Pass |

Original Machinery checks: 5,056 before and after. The original 17 CTest cases
remain; three additional cases cover DVI semantics, DVI CLI/interoperability,
and generated manual synchronization. All 49 portable catalog examples retain
their checks under both Machinery engines. Existing CHIP-8, SSAVM, closure,
generated-C, header and relocated SDK tests remain green.

| Coverage measure | Before | After |
|---|---:|---:|
| Catalog identities | 49 | 49 |
| Catalog native execution profiles | 7 | 8 |
| Catalog-only profiles | 42 | 41 |
| Legacy `vmspecs` identities | 38 | 38 |
| Legacy identities with native APIs | 4 | 5 |
| Legacy identities still without native execution | 34 | 33 |
| Legacy identities without a catalog mapping | 12 | 12 |

`make -C MetaVM test` and `ASAN_OPTIONS=detect_leaks=0
UBSAN_OPTIONS=halt_on_error=1 make -C MetaVM sanitize` also passed. Both include
the real TeX/DVItype rule and text-position oracles in this environment. Catalog
regeneration is byte-identical and leaves native code unchanged; the generated
manual check covers every legacy directory. `git diff --check -- MetaVM` passes.

Builds passed `-Wall -Wextra -Wpedantic -Werror` with no compiler warnings.
Ninja was absent, so builds used CMake's Unix Makefiles generator. Concurrent
Make sometimes printed FIFO-exists messages; these were not compiler warnings.
LeakSanitizer cannot inspect processes here; `detect_leaks=0` was required.
ASan memory-access and UBSan checks passed; leaks remain unverified.

GCC sanitizer static libraries were built with `-fPIC`, executables linked with
`-no-pie`, and CTest received `LDFLAGS=-no-pie` for its installed SDK consumer
builds. An initial `-fno-pie` static-library configuration could not link into
PIE SDK consumers and was corrected. Test development also corrected a callback
status expectation and NIL assembly syntax against the pre-existing Machinery
contract, as explained in `../REPORT.md`; no original regression was weakened.

The final logs in this workspace are `/tmp/metavm-vms-baseline/final-ctest.log`,
`/tmp/metavm-vms-clang-release/final-ctest.log`,
`/tmp/metavm-vms-gcc-san/final-ctest.log` and
`/tmp/metavm-vms-clang-san/final-ctest.log`.
See `../REPORT.md` for complete commands, API additions, sources and unresolved
implementation work. There is no claim that every VM or any complete language
platform has been implemented.
