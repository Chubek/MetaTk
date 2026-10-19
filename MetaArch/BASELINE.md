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
