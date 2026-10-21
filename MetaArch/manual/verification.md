# Nanobind bindings and manual verification

Dates: 2026-10-21–22. Scope: implement `bindings/python` with nanobind and add this
project manual. **No native API/ABI, schema, encoder data or generated backend
changes.** This is not completion of the broader A–G roadmap in `AGENTS.md`.

## Implementation

- C++17 nanobind module reuses `MetaArchAPI.hpp` and opaque ABI v1 ownership.
- Load/parse, metadata, lookup, backend load, all writers, named/form encoding,
  copied bytes/relocations, context cleanup and structured exceptions.
- Genuine int64 operands only, NUL name/path rejection, owned value copies,
  read-only metadata and GIL-held native calls.
- Optional CMake build/test/install, static/shared linkage, platform wheel build,
  Python enums, type stubs and PEP 561 marker.
- Manual chapters cover project structure/data flow, build/dependencies, schema,
  formats, CLI, encoders, relocation semantics, native/foreign APIs, Python, DSL,
  contribution workflow, tests and unresolved limitations.
- README/bindings/changelog now describe nanobind rather than the stale ctypes
  design. No `Library`, `Native`, runtime path discovery or METAARCH_LIBRARY API.

The requested `/mnt/nvme/third_party/nanobind` was absent. Verification used
`/mnt/nvme/metatk/third_party/nanobind`, supplied via `ARCHDB_NANOBIND_DIR`.
The build has no hard-coded dependency on either absolute path.

## Before changes

The default Release tree was reconfigured and built: 7/7 CTest cases passed in
11.90 s (DSL and CLI included). Its existing yyjson 0.10 checkout emitted a
CMake minimum-version deprecation notice; there were no compiler warnings.
The existing GCC sanitizer tree was inspected and re-run before edits:
4/4 native CTest cases passed in 62.79 s without findings.

Lint output from `build/archdb validate --lint ArchDB/*.dsymspec.yaml` exited 0
and was byte-identical to the existing snapshot
[`docs/python-bindings-baseline-lint.txt`](../docs/python-bindings-baseline-lint.txt).
The comparison copy is `/tmp/opencode/metaarch-nanobind-baseline-lint.txt`.

## Results

Environment: Linux x86-64, GCC 16.2.1, Clang 22.1.8, Python 3.14.7, Unix
Makefiles (Ninja unavailable). New trees used system libyaml/libxml2 and local
yyjson from `/mnt/nvme/joltfx/third_party/yyjson`. All four builds used
`-Wall -Wextra -Wpedantic -Werror` for both C and C++; zero compiler warnings.

| Build | Results |
|---|---|
| GCC Release, static core | 6/6 CTest cases, 13.79 s; includes 24 Python cases |
| Clang Release, shared core | 6/6 CTest cases, 12.93 s; includes 24 Python cases |
| GCC ASan+UBSan, static core | 5/5 native cases, 66.36 s; 24 Python cases separately, 0.188 s |
| Clang ASan+UBSan, static core | 5/5 native cases, 56.91 s; 24 Python cases separately, 0.182 s |
| CMake-installed Clang shared package | 24/24 Python cases, 0.036 s, without LD_LIBRARY_PATH |
| Installed GCC static-core wheel | 24/24 Python cases, 0.040 s |
| Default DSL-enabled build after changes | 7/7 CTest cases, 12.68 s |

No sanitizer findings. Python alone used `ASAN_OPTIONS=detect_leaks=0` with the
matching preloaded runtime; native tests retained normal leak checking. Release
results are end-to-end tests, not controlled performance benchmarks. Native
round trips retain 1307 checks, 0 failures over 28 specs/four formats; all
existing tests remain intact. Post-change lint is byte-identical to the baseline.
All local Markdown links in the 13 manual/Python-guide pages resolve. The C
manual example passes warning-clean syntax checking; the minimal YAML spec and
Python quick-start examples execute successfully.

The built wheel was `metaarch-1.0.0-cp314-cp314-linux_x86_64.whl` (282022 bytes).
Only the Python component is included. Codec system libraries are not bundled;
this is a local platform wheel, not a portable audited binary distribution.
No spec/backend output sizes changed; the writers themselves were unchanged.

## Reproduce

From the repository root (substitute paths/interpreter for your machine):

```sh
cmake -S . -B /tmp/opencode/metaarch-nanobind-gcc \
  -DCMAKE_BUILD_TYPE=Release -DARCHDB_BUILD_DSL=OFF \
  -DARCHDB_TEST_CPP_API=ON -DARCHDB_TEST_PYTHON_BINDINGS=ON \
  -DARCHDB_NANOBIND_DIR=/mnt/nvme/metatk/third_party/nanobind \
  -DFETCHCONTENT_SOURCE_DIR_YYJSON=/mnt/nvme/joltfx/third_party/yyjson \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build /tmp/opencode/metaarch-nanobind-gcc -j8
ctest --test-dir /tmp/opencode/metaarch-nanobind-gcc --output-on-failure
```

For Clang, use a new directory and add `-DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++ -DBUILD_SHARED_LIBS=ON`. Sanitizer trees use
RelWithDebInfo plus `-fsanitize=address,undefined -fno-omit-frame-pointer` in
**both** compiler flag strings. Run native CTest excluding Python and run Python
with the matching runtime preloaded as described in [testing](testing.md).

```sh
cmake --install /tmp/opencode/metaarch-nanobind-clang \
  --prefix /tmp/opencode/metaarch-nanobind-install
PYTHONDONTWRITEBYTECODE=1 \
  PYTHONPATH=/tmp/opencode/metaarch-nanobind-install/lib/python3.14/site-packages \
  python3 -m unittest discover -s bindings/python/tests -v

# Requires scikit-build-core; testing installed it in a temporary --target dir.
PYTHONPATH=/tmp/opencode/metaarch-nanobind-build-tools \
  CMAKE_BUILD_PARALLEL_LEVEL=6 python3 -m pip wheel \
  --no-build-isolation --no-deps ./bindings/python \
  -w /tmp/opencode/metaarch-nanobind-wheels \
  -Ccmake.define.ARCHDB_NANOBIND_DIR=/mnt/nvme/metatk/third_party/nanobind \
  -Ccmake.define.FETCHCONTENT_SOURCE_DIR_YYJSON=/mnt/nvme/joltfx/third_party/yyjson
python3 -m pip install --no-deps --target /tmp/opencode/metaarch-nanobind-wheel-site \
  /tmp/opencode/metaarch-nanobind-wheels/metaarch-1.0.0-cp314-cp314-linux_x86_64.whl
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=/tmp/opencode/metaarch-nanobind-wheel-site \
  python3 -m unittest discover -s bindings/python/tests -v
```

## Boundaries

The Python directory was empty at task start. Existing unrelated work in the
repository was preserved. The unfinished `daemon/` example was not changed.
Python wraps ABI-v1 spec operations, not the catalogue/DSL assembler. Calls hold
the GIL; free-threaded/subinterpreter use is unverified. Python 3.9 is the minimum
source dependency requirement, but only 3.14 was available for execution here.
Only Linux was tested. Standalone source distributions are not supported; wheel
builds require the enclosing native repository. See [current limitations](testing.md).
