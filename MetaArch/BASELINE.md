# Baseline (2026-10-19, before any edits)
Environment: Arch Linux, GCC 16.2.1, Clang 22.1.8, CMake 4.4.3, GNU Make
(ninja is **not installed** on this machine, so the "Unix Makefiles" generator
is used everywhere the brief says `-G Ninja`; no network access, so yyjson is
supplied to FetchContent from a local checkout via
`-DFETCHCONTENT_SOURCE_DIR_YYJSON=/mnt/nvme/joltfx/third_party/yyjson`
(yyjson 0.13.0). libyaml 0.2.5 and libxml2 come from system packages.)

## Release build (GCC)

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_YYJSON=/mnt/nvme/joltfx/third_party/yyjson
cmake --build build -j16        # ~13 s, zero warnings
ctest --test-dir build          # 3/3 tests, 11.9 s
```

* `test_archdb ArchDB ArchDBCatalogue.yaml`: **887 check(s), 0 failure(s),
  28 spec(s) x 4 formats** — matches the handover count.
* `archdb validate --lint ArchDB/*.dsymspec.yaml`: exit 0, 112 lines of notes,
  saved verbatim in `docs/baseline-lint.txt`.

## Release build (Clang)

```
cmake -S . -B build-clang -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DFETCHCONTENT_SOURCE_DIR_YYJSON=/mnt/nvme/joltfx/third_party/yyjson
```

Zero warnings; `test_archdb`: 887 checks, 0 failures.

## Sanitizer build (GCC, ASan+UBSan)

```
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DFETCHCONTENT_SOURCE_DIR_YYJSON=/mnt/nvme/joltfx/third_party/yyjson
ctest --test-dir build-asan     # 3/3 tests, 62.8 s
```

No sanitizer findings at baseline.

## Notes

* The compiler flags at handover are `-Wall -Wextra -Wpedantic -Wshadow`
  (library) **without** `-Werror`; adding `-Werror` is part of this work.
* Performance numbers for `tools/bench.c` are recorded in the same file family
  once the benchmark exists (Phase F).

## Python bindings baseline (2026-10-21)

Before adding `bindings/python`, separate shared-library trees were built in
`/tmp/opencode/metaarch-python-{release,asan}` with GCC 16.2.1, the local yyjson
checkout above, `ARCHDB_BUILD_DSL=OFF`, and
`-Wall -Wextra -Wpedantic -Werror`. Ninja is still unavailable; Unix Makefiles
was used. No build warnings.

- Release: 5/5 CTest cases passed in 14.32 s (C++17 ABI test enabled).
  The unchanged native round-trip suite reports 1307 checks, 0 failures.
- ASan+UBSan (`RelWithDebInfo`, `-fsanitize=address,undefined
  -fno-omit-frame-pointer`): 4/4 cases passed in 65.60 s, no findings.
- Current `archdb validate --lint ArchDB/*.dsymspec.yaml` output is preserved
  in `docs/python-bindings-baseline-lint.txt` (exit 0; existing warnings/notes).
  No native code or architecture data changes are needed for these bindings.

Verification after adding the bindings:

- GCC Release: 6/6 CTest cases, 13.99 s, including 22 Python tests.
- Clang 22.1.8 Release: 5/5 CTest cases, 12.79 s (C++ test disabled),
  including the same 22 Python tests. Both Release builds are warning-free
  under the flags above.
- Clang ASan+UBSan: 4/4 native CTest cases, 56.77 s, no findings.
- The 22 Python tests also passed against each GCC/Clang sanitizer library
  with the corresponding ASan runtime preloaded. Leak detection was disabled
  only for the Python process (`ASAN_OPTIONS=detect_leaks=0`), not native CTest.
- A pure-Python wheel was built offline, installed into an isolated target
  directory, and all 22 tests passed against the CMake-installed shared library.

Reproduce the build using the flags above, adding
`-DARCHDB_TEST_PYTHON_BINDINGS=ON -DBUILD_SHARED_LIBS=ON` for Release testing;
see `bindings/python/README.md` for standalone test/install commands. For Linux
sanitizer testing with GCC, run the standalone Python command with
`LD_PRELOAD="$(gcc -print-file-name=libasan.so)" ASAN_OPTIONS=detect_leaks=0`;
use `clang -print-file-name=libclang_rt.asan-x86_64.so` for Clang on x86-64.

## Nanobind implementation (2026-10-21)

The prior Python notes above describe the earlier ctypes design; its package
directory was empty when the nanobind task began. Current implementation and
reproducible results are in [manual/verification.md](manual/verification.md).

- Before edits: default Release 7/7 CTest, 11.90 s; existing GCC sanitizer tree
  4/4, 62.79 s. Existing lint snapshot is unchanged byte-for-byte.
- After: GCC static Release 6/6, 13.79 s; Clang shared Release 6/6, 12.93 s.
  Both include 24 Python unittest cases; all existing native tests are intact.
- GCC ASan/UBSan native 5/5, 66.36 s; Clang 5/5, 56.91 s. Python tests also
  pass against both instrumented extensions with matching runtime preloaded.
- All four builds are warning-clean with `-Wall -Wextra -Wpedantic -Werror`.
- CMake shared installation and a built/installed platform wheel each pass all
  24 Python tests. Native API/ABI, specs, backends and codecs are unchanged.

## ArchBFD addition (2026-10-22)

The ArchBFD object-format catalogue added five CTest cases to the tree
(`archbfd_catalogue`, `archbfd_schema_check`, `archbfd_data_check`,
`archbfd_extractor_check`, `archbfd_cli`).

- GCC Release: 12/12 CTest, 12.5 s. GCC ASan+UBSan (`RelWithDebInfo`,
  `-fsanitize=address,undefined -fno-omit-frame-pointer` for C *and* C++):
  12/12, 63.8 s, no findings.
- New checks: `test_archbfd` reports 3463 checks, 0 failures
  (9 documents x 4 formats). The pre-existing suites are unchanged
  (1307 architecture, 292 form-encoding, 289 relocation checks).
- The extractor check compares the committed `ArchBFD/*.bfd.yaml` with a
  fresh extraction from the `binutils/` checkout; it fails when they differ.
