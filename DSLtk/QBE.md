# QBE backend

MetaML's C++ and D runtimes can execute through an optional, host-native QBE
backend. It supports the same language as XVM, including closures, tail calls,
pattern matching, modules/functors, records, comprehensions and host primitives.
The default engine remains XVM.

## Build and use

```sh
cmake -S . -B build-qbe -DMETAML_ENABLE_QBE=ON \
  -DMETAML_QBE_DIR=/mnt/nvme/metatk/third_party/qbe
cmake --build build-qbe -j2
build-qbe/metaml --engine qbe --no-stdlib -e '40 + 2'
ctest --test-dir build-qbe -R 'metaml_qbe|metalib_toolchain' --output-on-failure
```

The default QBE source location is the sibling `../third_party/qbe`. CMake builds
QBE out of tree without modifying its sources. Alternatively set
`METAML_QBE_EXECUTABLE=/path/to/qbe` to use an existing compiler. The backend
currently requires a native POSIX build (Linux/macOS, amd64/arm64/riscv64 as
supported by QBE). Execution has been tested on Linux amd64.

At runtime it needs QBE and a host C compiler driver/assembler/linker. Defaults
are the build's QBE and C compiler; if the build-tree QBE no longer exists, it
searches `PATH` for `qbe`. `METAML_QBE` and `METAML_QBE_CC` override the executable
paths. Each variable is **one executable**, not a shell command or flag list.
Installed deployments should set these variables or put `qbe` and `cc` on PATH.
Use `TMPDIR` to select the writable temporary filesystem used for compilation.

The CLI's `--engine qbe` works in `metaml`, `metaml-repl-cpp` and the D CLI.
Without the optional backend, evaluating with this engine reports M0409 rather
than silently using another engine. Checking and AST-only operations do not
require QBE. `METAML_NO_JIT` also disables QBE execution.

## Embedding

Link C++ applications against the CMake `DSLtk` target from a QBE-enabled build;
it propagates `METAML_ENABLE_QBE` and the optional `metaml-qbe` static library.

```cpp
#include <metaml-backends/QBE.hpp>

metaml::Session session;
metaml::backends::QBE backend;
auto result = backend.eval(session, "40 + 2", "example.mml");
```

The adapter temporarily selects QBE and restores the previous engine even on
failure. To select it persistently, set
`session.interp().engine = metaml::ExecutionEngine::QBE` instead. Source loading,
imports, primitive callbacks and worker interpreters then inherit that choice.

D exposes the same `ExecutionEngine.QBE`, plus `metaml_backends.QBE.QBE` in
`metaml-backends/QBE.d`. Manual D builds need the `METAML_ENABLE_QBE` version,
`libmetaml-qbe.a`, and the platform C++ runtime/dynamic-loader libraries (for
example, LDC on Linux: `-d-version=METAML_ENABLE_QBE libmetaml-qbe.a -L-lstdc++
-L-ldl`). Include the adapter's `.d` source explicitly when importing it, just as
for the existing `metaml-backends/XVM.d` adapter. CMake wires these flags into the
D CLI and tests automatically. Ordinary builds still need no QBE dependencies.

## Lowering and runtime contract

The backend reuses MetaML's checked XVM lowering and value/application ABI, not
the tree evaluator. For each compiled expression it emits QBE SSA functions for
basic blocks of up to 32 specialized runtime-helper calls. Each helper's nonzero
return exits the native block immediately, preserving branches, tail transfers
and exceptions. QBE performs instruction selection, register allocation and
native code emission. Boxed operations still run in the shared semantic helpers;
this is not unboxed numeric lowering or a standalone executable/AOT format.

QBE runs in a separate process, followed by the host assembler/linker, without a
shell. One private shared library is produced per compiled expression, loaded
locally, and its files are removed after loading (also on failure). Modules live
with the interpreter's code cache; active executions retain their code across
nested evaluation and tail transfers. D `Session.close()` and
`interp.clear_xvm_cache()` explicitly release cached QBE modules.

Cold compilation is considerably more expensive than the built-in XVM JIT,
especially when loading a large standard library. Native pointers in the IL are
process/session-specific; artifacts are not portable or reusable across runs.
There is no interpreter fallback on QBE/compiler/loader errors (M0409).

Existing instruction, operand-stack, depth, cancellation and permission checks
remain active. `xvm_native_limit` limits each expression's **complete shared
library file size**, a conservative bound including object metadata and linker
overhead; exceeding it reports M0406 before loading. Zero disables native QBE
execution. Compiler subprocess time/memory and total cache storage are not
bounded by this limit; this backend is not a sandbox for untrusted compilation.
It does not grant guest primitive capabilities or execute guest source in the
compiler process. Use only trusted compiler executables.

`interp.qbe_native_blocks` counts executed QBE blocks; these also increment
`xvm_native_blocks` and the ordinary `xvm_instructions` counter. Separate native
caches allow switching between QBE, built-in native XVM and portable execution.
