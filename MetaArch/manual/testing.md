# Testing, troubleshooting and limitations

## Test layers

| Test/source | Coverage |
|---|---|
| `archdb_roundtrip`, `tests/test_archdb.c` | All 28 specs/four codecs, tree/model comparisons, diagnostics, catalogue, table-driven encodings |
| `archdb_encode_form` | Checked operand mapping, bounds, aliases and failure cases |
| `archdb_relocations` | Bit intervals, offsets, endianness and invalid metadata |
| `metaarch_c_abi`, `metaarch_cpp_api` | Opaque handle/result lifetimes and independent C++17 use |
| `metaarch_dsl`, `metaarch_cli` | Optional DSL/assembler and frontend behavior |
| Optional D/Go tests | Cross-language ABI and copied values |
| `metaarch_python_api` | 24 unittest cases, synthetic fixture/backends plus all shipped specs |
| `tests/check_encodings.py` | Optional independent llvm-mc encoding checks |

```sh
ctest --test-dir build --output-on-failure
ctest --test-dir build-python -R metaarch_python_api --output-on-failure
python3 tools/compile_archdb.py --check
```

Full backend round trips are more expensive than the focused suites. CTest
currently groups them in one native test, not one per spec. No backend-skipping
sanitizer option or completed libFuzzer/benchmark budget infrastructure is claimed.

## Warning-clean and sanitizer builds

Use separate build directories for compilers/configurations:

```sh
cmake -S . -B build-check -DARCHDB_BUILD_DSL=OFF \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DARCHDB_TEST_PYTHON_BINDINGS=ON \
  -DARCHDB_NANOBIND_DIR=/path/to/nanobind \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build build-check -j
ctest --test-dir build-check -E metaarch_python_api --output-on-failure
LD_PRELOAD="$(gcc -print-file-name=libasan.so)" ASAN_OPTIONS=detect_leaks=0 \
  PYTHONPATH="$PWD/build-check/bindings/python" \
  python3 -m unittest discover -s bindings/python/tests -v
```

The Python executable is not ASan-instrumented, so preload the matching runtime
on Linux. Use Clang's `libclang_rt.asan-x86_64.so` when built with Clang on x86-64.
Leak detection is disabled only for the Python process, not native CTest; native
ASan/UBSan still checks extension operations. Do not apply LD_PRELOAD to the
entire build process. Portable sanitizer/runtime integration varies by platform.

## Troubleshooting

| Symptom | Action |
|---|---|
| `DSLtk.hpp not found` | Set `ARCHDB_DSLTK_DIR` or disable DSL |
| Missing yyjson/network unavailable | Install system package or supply local FetchContent source |
| Missing nanobind package | Set `ARCHDB_NANOBIND_DIR` to recursive checkout or install nanobind for selected Python |
| Missing Python development module | Install interpreter headers; select matching `Python_EXECUTABLE` |
| `No module named metaarch._native` | Import from built/installed package, not source-only package |
| `libmetaarch.so` not found | Static build avoids it; shared installs need correct RPATH/loader path |
| Wrong Python ABI suffix | Rebuild for the interpreter actually importing the module |
| Form count/operand mismatch | Compare visible operands against first-use parameters; don't bypass checks by guessing |
| Range failure with a register name | Resolve to `register(name).encoding`; strings are not integer operands |
| Backend file not found | Preserve its path relative to spec source; DSL backend symlinks need original ArchDB |
| ASan runtime not first | Preload the runtime matching the extension compiler |

## Known limitations

The larger hardening roadmap is unfinished. General hostile-input limits,
uniform OOM hooks, compiler bit built-ins/static interval queries, comment-safe
YAML formatting, central UTF-8 fixes, schema reflection/output schema, semantic
diff, triple/alias catalogue lookup, structured diagnostics, fuzzing/CI matrix,
installed CMake package and comprehensive deterministic golden tests remain
separate work. Warnings/notes can expose unresolved data gaps; passing a load
does not independently certify instruction bytes. See the encoder audit before
relying on affected instructions. Python specifically excludes catalogue/DSL,
register-name operands, parallel native execution and free-threaded support.
