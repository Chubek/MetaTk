# Runnable manual examples

These are small complete SDK consumers, not pseudocode. Run the following from
`MetaToolchain/MetaVM` after building `out-manual` as in chapter 5:

```sh
cmake --install out-manual --prefix "$PWD/out-manual-sdk"
cmake -S manual/examples -B out-manual-examples \
  -DCMAKE_PREFIX_PATH="$PWD/out-manual-sdk" \
  -DMETAVM_MANUAL_VMWEAVE="$PWD/out-manual-sdk/bin/vmweave"
cmake --build out-manual-examples -j4
ctest --test-dir out-manual-examples --output-on-failure
```

Use a fresh prefix/build tree rather than overwriting an unrelated SDK.
`METAVM_MANUAL_VMWEAVE` is optional: without it the five embedding/authoring/native
tests build, but the generated-C factory example is omitted. An explicit path
avoids accidentally invoking another installed version's compiler.

| File | Built executable | Demonstrates |
|---|---|---|
| [embed.c](embed.c) | `manual_embed_c` | C11 compile/run/cleanup under both engines |
| [embed.cpp](embed.cpp) | `manual_embed_cpp` | C++11 wrapper and rooted objects |
| [host.c](host.c) | `manual_host` | Typed host callback, borrowed user data, defined arithmetic |
| [authoring.cpp](authoring.cpp) | `manual_authoring` | Builder and ToyR16 Definition lowering |
| [native.cpp](native.cpp) | `manual_native` | All eight native execution profiles and DVI replay |
| [factory_host.c](factory_host.c) | `manual_factory` | C factory emitted from `answer.mvm` |
| [answer.mvm](answer.mvm) | Input file | Zero-argument Machinery program returning 42 |
| [answer.ssa](answer.ssa) | Input file | SSAVM scalar expression returning 42 |

The first three programs print `42`; the authoring program prints
`builder=42 definition=42 memory[3]=42`. The native program reports that its
profile checks passed. The optional factory program prints `42`.

`manual_authoring [OUTPUT.c]` can additionally emit `manual_toy_factory` for its
specialized ToyR16 program. That factory requires the two initialized/rooted
state arrays described in chapter 21; the generic zero-argument runner is not
its host. For that exact C setup see the project's existing
[Definition factory regression](../../tests/definition_factory.c).

Additional CLI checks:

```sh
out-manual/Machinery/metavm-run manual/examples/answer.mvm
out-manual/ssavm-run manual/examples/answer.ssa
out-manual/ssavm-run --check manual/examples/answer.ssa
```

Expected results: `42`, `42`, and `verified`. These examples do not require
optional MetaML, MetaTeX, libdrm, fonts, display hardware, or network access.
The native example's zero-page DVI document avoids a metrics callback while
still exercising structural decoding and verified Machinery replay.

## Validation of this edition

On 2026-11-04, all six example tests passed against an installed SDK with
GCC 16.2.1 Release, Clang 22.1.8 Release, and Clang Debug ASan/UBSan with
default leak detection. Builds used `-Wall -Wextra -Wpedantic -Werror`;
the core wrapper example compiled as C++11 and the framework examples as C++17.
The assembly/SSAVM inputs and the chapter 22 MetaML snippet were also executed,
and the optional authoring C output compiled as C11. This evidence applies to
these examples, not to unbuilt MetaTeX/libdrm integrations or other platforms.

[Back to the manual](../README.md)
