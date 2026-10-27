# GPPX — General-Purpose Programmable Preprocessor eXtended

GPPX is a C++17 preprocessing library and CLI with a configurable directive grammar, recursive macros, nested conditionals, includes, embedded resources, text rewrites, and **native shared-library plugins**. The original archive was a scaffold; this release includes a buildable engine and implemented extension-category APIs.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/gppx examples/demo.gppx
./build/gppx --plugin ./build/plugins/gppx_webber.so examples/webber/demo.gppx
cmake --install build --prefix "$HOME/.local"
```

C++17 compiler and CMake >=3.16 required. No external library is needed for the core except the platform's dynamic loader on Unix. Tested with GCC 14 on Linux. Other platforms are supported in the build configuration but are **not verified** in CI here.

## First example

```gppx
#define COUNT 3
#define ADD(x,y) ((x)+(y))
#if defined(COUNT) && COUNT > 0
#for n in 1..COUNT
Row n => ADD(n,10)
#endfor
#endif
```

```sh
./build/gppx -DCOUNT=5 -Iexamples/include examples/demo.gppx
./build/gppx --profile template examples/template/example.gppx
./build/gppx --macros dollar -e '#define GREETING Hello
$(GREETING)'
```

Profiles: `c` (`#directive`, bare identifier macros), `percent` (`%directive`), `template` (`@directive`, `{{MACRO}}`). Change syntax *during preprocessing* with `#syntax prefix %` or `#syntax profile template`; plugins can add syntax options.

## Implemented features

- Object-like, function-like, variadic and multiline macros; argument stringification (`#`) and token concatenation (`##`); recursion protection; macro push/pop.
- `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`, `#ifmatch`, `#elifmatch` and `defined(NAME)`; expression operators, short-circuiting and ternary selection.
- `#include`, `#include_once`, `#import`, `#pragma once`, path search, cycle detection, recursion/output limits.
- `#repeat`, `#for`, `#raw`, `#emit`, `#eval`, `#assert`, `#error`, `#warning`, `#line`, `#region`, `#endregion`.
- `#embed` formats `hex`, `c_array`, `string`, `base64` and plugin-supplied encoders; regex `#rewrite` and `#clear_rewrites`.
- Extension points for directives, callable macros, line rewriters, readers, sinks, embeds, syntax options, and pattern matchers; C ABI version 1 and C++ registration API.
- CLI input/output, CLI definitions, configuration subset, interactive REPL, diagnostics and C API.
- A loadable Webber plugin with directives, callable macros, two-pass assembler embeds and pattern matching.

## Project layout

- `GPPX.cpp`: core engine and ABI; `GPPX.c`: compiled C extension helpers.
- `include/GPPX.h`: stable C ABI; `include/GPPX-all.h` and eight `GPPX-*.h` category APIs; `include/GPPX.hpp`: C++ engine.
- `driver/`: CLI, extended REPL, deterministic plugin-directory loader, configuration parser for a YAML subset.
- `builtin/`: builtin registry and category documentation.
- `plugins/webber/`: native plugin with two-pass example VM assembler.
- `examples/`: preprocessing and embedding samples.
- `tests/`: executable tests; `scripts/`: build and test utilities.
- `docs/`: language guide, architecture, plugin authoring, security and known limitations.

## Maturity and important constraints

This is a **working implementation**, not a formally verified or compatibility-complete implementation of the ISO C preprocessor. Macro rescanning, numeric semantics, line splicing, comments and directive grammar are GPPX-specific. A native plugin runs arbitrary code in the host process; only load plugins you trust. Configuration uses a **documented YAML subset**, not arbitrary YAML. Binary-safe embedding is supported in C++, but the simple C `char**` processing interface is text-oriented. Resource limits exist but do not constitute a hardened sandbox.

See [`docs/LANGUAGE.md`](docs/LANGUAGE.md), [`docs/EXTENSIONS.md`](docs/EXTENSIONS.md), [`docs/PLUGIN_GUIDE.md`](docs/PLUGIN_GUIDE.md), [`docs/WEBBER.md`](docs/WEBBER.md), and [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Prebuilt binaries (optional)

The archive includes `prebuilt/linux-x86_64/` with a release CLI, shared library, public headers and the Webber plugin. On compatible x86-64 Linux systems, run:

```sh
./prebuilt/linux-x86_64/bin/gppx --version
./prebuilt/linux-x86_64/bin/gppx --plugin ./prebuilt/linux-x86_64/lib/gppx/plugins/gppx_webber.so examples/webber/demo.gppx
```

These binaries are Linux/glibc-specific and are not guaranteed to work on another distribution, CPU, or ABI. Rebuild from source as needed.

## Working with the previously stubbed files

The extension headers are no longer empty aliases. All eight include callable
registration adapters and dedicated APIs. Their implementation is in `GPPX.c`,
compiled into the same `libgppx` as the core. The source-tree `GPPX.h` is an
umbrella; installed clients should include `<GPPX-all.h>` for all extension
categories or `<GPPX.h>` for the minimal stable engine API. See
`docs/EXTENSIONS.md` and `tests/test_extensions.c`.

```c
#include <GPPX-all.h>
#include <stdio.h>
/* Pure C, link with -lgppx */
GPPX_State *state = gppx_create();
if (!state) return 1;
if (gppx_macro_define_spec(state, "HELLO=world")) return 1;
char *output = NULL;
if (gppx_process(state, "HELLO\n", "<example>", &output)) return 1;
fputs(output, stdout);
gppx_free(output);
gppx_destroy(state);
```

To scan plugin directories use `gppx --plugin-dir ./build/plugins --eval
'#wbr_upper hello'`. For a multiline REPL use `:begin` and `:end`. Configuration
is intentionally a small YAML subset; see `driver/config.yaml` for the exact
supported mapping/sequence conventions.

### Additional examples and tooling

- `build/gppx_mylang [source.my]` demonstrates a custom language front end
  using the style, UTF-8 input and qualified-symbol helpers. With no argument,
  it prints a built-in demonstration. See [extension APIs](docs/EXTENSIONS.md).
- [VM reference specifications](VMSpec/README.md) expand to JSON under
  `build/VMSpec/json/`; `GPPX_BUILD_VMSPECS=OFF` disables this build target.
  These are documented semantic subsets, not executable VM implementations.
- `sh addons/install.sh [--prefix VIM_RUNTIME_DIR]` installs the
  [Vim syntax and indentation support](addons/README.md).
