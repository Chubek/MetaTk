# MetaAS DSL baseline — 2026-10-25

Scope: native `MetaAS-DSL.hpp` and the existing C++ ownership wrapper. The inherited
MetaArch-wide hardening brief is not a claim that this change implements phases
A–G; no architecture data, encoders, codecs or C ABI definitions changed here.

Baseline taken from the clean MetaToolchain working tree before source edits.
Builds used a source copy in `/tmp/metaas-dsl-work`, because this session initially
allowed writes only to ArchAS and `/tmp`. CMake used Unix Makefiles: the requested
Ninja executable is not installed. Dependencies were existing local/system copies.

| Baseline | CTest results | Wall time |
| --- | --- | --- |
| GCC Release | 32/32 passed | 13.30 s |
| Clang Debug, ASan + UBSan + leak detection | 32/32 passed | 93.98 s |

GCC 16.2.1 (20260810), Clang 22.1.8. Both builds enabled
`-Wall -Wextra -Wpedantic -Werror`; no compiler warnings were emitted. Sanitizer
builds additionally used `-fsanitize=address,undefined -fno-omit-frame-pointer`.
The first sandboxed sanitizer run could not run LeakSanitizer under ptrace; it
was repeated outside the sandbox with leak detection enabled and passed.

Existing focused checks: 72 ArchAS API checks, 95 C embedding checks, and 32 C++
embedding checks. The old 887-check MetaArch handover number is not used as a
baseline for this newer tree. Full suites included the backend round trips.

Raw baseline logs for this session:
`/tmp/metaas-baseline-build.log`, `/tmp/metaas-baseline-test.log`,
`/tmp/metaas-asan-baseline-build.log`, `/tmp/metaas-asan-baseline-test.log`.
