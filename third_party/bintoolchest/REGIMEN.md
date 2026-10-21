# Tourchstone refinement regimen

## Validation results

On Linux AMD64 with GCC 16.2.1:

- Debug/static: **16/16 CTest cases passed**.
- Release/shared with `-Werror`: **16/16 passed**; assertions remain enabled in
  the native regression tests even when the libraries use `NDEBUG`.
- AddressSanitizer + UndefinedBehaviorSanitizer: **16/16 passed**.
- Broader DomBind suite: **17 passed, 2 skipped** out of 19. Both skips require
  unavailable JDK/JNI headers. D, Go, Lua, Node, OCaml, Ruby and Rust runtime/module
  checks and installed CMake consumers passed.
- Go initially failed because the shell's `GOROOT` referenced a nonexistent
  installation. Re-running with `GOROOT=/usr/lib/go` selected the actual SDK and
  passed with vet enabled; no test was disabled to hide the environment error.
- All 11 `.xbind` files present here were updated and validated. Jiterati's local
  file lives under a gitlink: its contents must be preserved/versioned in that
  optional checkout, not mistaken for a parent-repository tracked file.

## Acceptance gates

| Area | Executable checks |
| --- | --- |
| C API | XBind target lookup, invalid arguments, spec resolution priority and short buffers; directories rejected as spec files. |
| Spec contract | All `.xbind` files present in this checkout load under stock Lua and QaMRpp; malformed arrays, duplicate entries, invalid identifiers/types/nullability and unsupported modules fail. |
| Spec IR | Deterministic JSON with typedefs, target options, ownership/nullability and feature tags; constructor option tables are not mutated. |
| ABI agreement | Exact C++ function-pointer signature assertions compile for every nonempty repository C spec; deliberately mismatched signatures fail. |
| Lua generator | Real DomLuaBind headers, default/library/target override precedence, corrupt map rejection, integer ranges, exact arity/types, nullable pointers and embedded-NUL errors. |
| Generated modules | Hello, XBind self-binding, DynLOAD, DynINVOKE and aggregate Tourchstone compile and execute. |
| Loader | Inspection failure, flag rejection, missing symbols, size-probe buffers, environment-owned close, promoted-symbol invalidation and ownership-safe promotion. |
| Invoker | Invalid/cyclic/void-field types, failure-path allocation ownership, copied type lifetimes, null argument storage, mixed integer/FP returns, hidden returns and closure lifetime/metadata. |
| Cross-ABI layout | Existing ABI cases plus AAPCS64 stack strides and explicit rejection of unsupported indirect Win64/AAPCS64 arguments. |
| Build modes | Debug/static, Release/shared with warnings as errors, ASan/UBSan, and native runtime binding SDK tests. |

## Reproduction

Run from the libstone root. All output directories are outside the source tree.

```sh
cmake -S tourchstone -B /tmp/opencode/tourchstone-refine \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/opencode/tourchstone-refine --parallel 3
ctest --test-dir /tmp/opencode/tourchstone-refine --output-on-failure

cmake -S tourchstone -B /tmp/opencode/tourchstone-release \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_C_FLAGS=-Werror -DCMAKE_CXX_FLAGS=-Werror
cmake --build /tmp/opencode/tourchstone-release --parallel 3
ctest --test-dir /tmp/opencode/tourchstone-release --output-on-failure

cmake -S tourchstone -B /tmp/opencode/tourchstone-sanitize \
  -DCMAKE_BUILD_TYPE=Debug \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined \
  -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address,undefined
cmake --build /tmp/opencode/tourchstone-sanitize --parallel 3
ctest --test-dir /tmp/opencode/tourchstone-sanitize --output-on-failure

PYTHONPATH="$PWD/tourchstone/xbind/dombind/domqlib" TMPDIR=/tmp/opencode \
  python3 -B -m unittest discover -s tourchstone/xbind/dombind/tests -v
```

For a Go distribution without the `vet` executable, the existing DomBind suite
supports explicit `DOMBIND_GO_VET=off`. This bypasses vet only, not Go compilation
or runtime tests. Missing SDKs are reported as skips, never substituted with fake
headers. Compiler, linker and runtime failures remain failures.

## Scope and remaining limits

- Lua is the only implemented XBind emitter. The other eight targets retain
  their spec/type-map/runtime inventories, not generated-binding guarantees.
- JSON is a spec IR, not a parsed C header AST. Compiler assertions verify exact
  selected signatures; general header parsing/source spans remain incomplete.
- Records, callbacks, automatic out-buffer/string marshalling and automatic
  ownership/finalizers remain unimplemented. Explicit opaque handles require
  correct caller-managed lifetimes; they do not enforce pointer provenance.
- Yellowstone's out-string spec is signature-checked but rejected by Lua
  emission. Firestone and the optional Jiterati checkout are explicitly C++
  header inventories; no C ABI or executable empty module is invented.
- Cross-ABI layouts are not a claim of full native ABI conformance. This pass's
  execution tests run on Linux AMD64; other architectures need native execution
  coverage. Variadic calls, packed/vector/long-double types are not modeled.
- Jiterati is a gitlink. Its local `.xbind` inventory is validated when present;
  the optional checkout need not be populated to build Tourchstone.

See [xbind/MANUAL.md](xbind/MANUAL.md) for the IR, generator and ownership contract.
