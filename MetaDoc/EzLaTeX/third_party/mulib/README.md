# MuLIB

A C11 modular single-header utility library with **198 implemented modules**.
Inspired by the simplicity of CTL and Klib; code is original, not vendored from the provided archives.

## Correct implementation-switch syntax

The example shorthand `#MULIB_IMPL_STACK` is not valid C preprocessing syntax.
Use `#define MULIB_IMPL_STACK` **before** `#include <mulib-stack.h>`.

```c
#define MULIB_IMPL_SEXPR
#define MULIB_IMPL_MANPAGE
#define MULIB_IMPL_MICROTCL
#define MULIB_IMPL_ARGPARSE
#define MULIB_IMPL_STACK
#include <mulib-sexpr.h>
#include <mulib-manpage.h>
#include <mulib-microtcl.h>
#include <mulib-argparse.h>
#include <mulib-stack.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    mulib_sexpr_t *sexpr = mulib_sexpr_cont();
    mulib_manpage_t *manpage = mulib_manpage_linux();
    mulib_microtcl_t *microtcl = mulib_microtcl_loadall();
    mulib_argparse_t *args = mulib_argparse_load();
    mulib_stack_t *stack = mulib_stack_create();
    /* Use objects, then release in reverse or any safe order. */
    mulib_stack_destroy(stack);
    mulib_argparse_destroy(args);
    mulib_microtcl_destroy(microtcl);
    mulib_manpage_destroy(manpage);
    mulib_sexpr_destroy(sexpr);
    return 0;
}
```

Build: `cc -std=c11 -Iinclude examples/quickstart.c -lm -o quickstart`.
Run tests: `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`.

## Integration

For multi-translation-unit use, define each implementation macro in exactly one `.c` file and include normal declarations everywhere else. Do not define implementation macros in globally distributed compiler flags. Headers should compile standalone. C++ clients should compile at least the implementation TU as C11 unless they specifically test C++ compatibility.

Modules reuse each other. Define the implementation switches for **all transitive
dependencies** before including any of their headers; dependency switches are
never enabled implicitly. Each manual and standalone example lists the required
switches. For example, the JSON parser uses MuLIB parser combinators, UTF-8
validation, and string builders:

```c
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#include <mulib-json.h>
```

YAML, TOML, templates, and JSON-RPC share the JSON value model instead of
maintaining competing trees. Other composition paths include allocation
tracking with memory pools, shared grammar/forest/stack machinery, and markup
builders and queries over a shared document model. See [the module guide](manual/modules.md).

Scannerless [PEG/EBNF loaders](manual/mulib-parsec.md#owned-grammar-loading)
build combinator parsers with named semantic actions and owned
[ASTs](manual/mulib-ast.md). Trees support S-expression, JSON and YAML output,
visitors, listeners and pull events. [Confuse](manual/mulib-confuse.md) adds
libconfuse-style configuration; [lazy streams](manual/mulib-lazystream.md)
provide on-demand map/filter/take/drop/fold pipelines.

## Scope and limitations

The term *header-only* here means no prebuilt or separately shipped `.c` files; implementations live in headers behind feature switches. These modules implement focused, documented APIs, not every feature implied by a format's name. In particular, configuration, markup, terminal, protocol, and grammar modules document their supported syntax and limits; they are not claims of full YAML, TOML, XPath, HTML5, CSS, CommonMark, VT, or DBus conformance. Unsupported features must not be mistaken for supported conversions. These modules are not complete substitutes for a production Tcl distribution, POSIX manuals toolchain, JSON Schema validator, full Unicode database or cryptographic engine. `mulib-microtcl.h` `loadall()` registers **all MuLIB MicroTcl built-ins**, not external Tcl packages. Documented support and actual APIs determine feature completeness. All names and responsibilities are enumerated in `MANIFEST.yaml`; every header has a dedicated manual in `manual/` and embedded Doxygen text.

There are no mandatory third-party library dependencies. Wayland/Xlib connection
helpers optionally load their native client runtimes; wlroots helpers provide
portable geometry/layout operations, not a compositor ABI binding.
Platform modules use operating-system
APIs and document platform restrictions; dynamic loading may need `-ldl`, and
numerical modules may need `-lm`. DRM refers to Linux Direct Rendering Manager.
Python and Ruby modules **generate ctypes/Fiddle binding source** rather than
embedding or linking an interpreter. Using their generated source naturally
requires the target language runtime and the described C shared library.

The networking extensions share DNS and HTTP models and expose bounded,
transport-independent client/server cores. Applications supply transport and
authentication policy; these modules do not implicitly add TLS or turn protocol
state machines into production-ready Internet services. Task pools and parallel
helpers use native threads; link with `-pthread` on POSIX systems (the CMake
`mulib` target propagates the thread dependency).

Compiler utilities cover checked MIR execution, rewriting, selection, scheduling,
emission, relocation and a focused native JIT. Architecture metadata comes from
the repository's [`dsymspec/`](dsymspec/) directory, not an external architecture
database. Consult individual manuals for supported instruction encodings and
platforms. A checked-in metadata snapshot keeps builds dependency-free;
`python3 tools/check_snapshot.py` verifies its source/schema hashes. Full
regeneration uses the optional developer-only PyYAML and jsonschema packages.
`mulib-inet6.h` is the canonical IPv6 header; `mulib-inte6.h` remains
compatible. The existing spellings `impaclient` and `dnsnmeserver` are retained.

All headers have implementations. The [dependency strata](manual/strata.md) index
records every direct dependency and integration layer. New focused modules include
a bounded i32 WebAssembly runtime (`wask`), a numeric Lua subset (`mune`), inherited
method dispatch (`mop`), and backend-neutral window actions (`wmfunc`). Their manuals
state the supported profiles; these names do not imply full standards coverage.

## Licensing

MuLIB's new source is MIT-licensed, with no code copied from the supplied CTL and Klib reference archives.

## Extensive examples and test coverage

Every module has a dedicated manual in `manual/` and an independently compilable
C example in `examples/modules/`. Run
`python3 tools/check.py` to compile all examples and focused tests with
`-O2 -Wall -Wextra -Wpedantic -Werror`, check headers independently, and test combined implementations
and declaration-only C/C++ inclusion. Use `CC=clang CXX=clang++` for Clang or
add `--sanitizers` for ASan/UBSan.
Optionally use `cmake -S . -B build -DMULIB_BUILD_MODULE_EXAMPLES=ON` to build and run them
under CTest. `tests/functional.c` covers multi-module usage and representative edge cases;
`tests/all_headers.c` checks that all implementations can coexist in a single C translation unit;
`tests/declarations.c` checks declaration-only inclusion.
Focused `tests/test_*.c` suites cover the new libraries. CMake also supports
`-DMULIB_SANITIZERS=ON`; test working directories are isolated for parallel CTest runs.
`tests/composition.c` verifies JSON → YAML → TOML → template composition and
tracked allocation/container/vector usage through a separate implementation
translation unit. `examples/dogfood.c` demonstrates parser-backed JSON feeding
vector algebra.

See [header hardening](manual/hardening.md) for failure-injection tests,
deterministic hostile-input coverage, optional libFuzzer builds and strengthened
ownership/validation contracts. These checks are not a security certification.

[OIX](manual/mulib-oix.md) compiles C, JSON, YAML, TOML, or Confuse configuration
into a tiny standalone x86 OS image, with cooperative tasks, console I/O, and
embedded read-only files. Its optional Linux x86 KVM runner needs no guest Linux,
BIOS, or external runtime library. See [the compiler/runner example](examples/modules/oix.c).
