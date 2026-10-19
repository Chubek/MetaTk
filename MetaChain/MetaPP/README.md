# MetaPP

MetaPP is a standalone C++20 preprocessing library and command-line tool. It
combines C-style preprocessing with explicit, in-process metaprogramming hooks.
There are no third-party build or runtime dependencies.

## Build and test

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --install build --prefix /your/prefix
```

The C and C++ API tests run without a testing framework. If Python and a system C
compiler are available, CTest also runs CLI integration and differential tests
against that compiler. `BUILD_SHARED_LIBS=ON` builds a shared library for FFI.

Consumers can use `add_subdirectory(path/to/MetaPP)` or install the package and use:

```cmake
find_package(MetaPP CONFIG REQUIRED)
target_link_libraries(my_program PRIVATE MetaPP::MetaPP)
```

## Command line

```sh
build/metapp -I include -DDEBUG=1 -UOLD input.c -o output.c
build/metapp examples/pipeline.c
```

Input defaults to stdin, output to stdout; `-` explicitly selects either stream.
`-D`/`-U` operations are applied in command-line order. Both joined (`-Iinclude`)
and separate (`-I include`) arguments work. `-E` and `-P` are accepted; output
always omits line markers. Errors go to stderr and return status 1. An input
error is detected before opening the output file.

## Preprocessing

Implemented features:

* Preprocessing tokens, comments, CRLF normalization, backslash-newline splicing,
  literal prefixes, preprocessing numbers, and directive/paste digraphs.
* Object/function macros, recursive suppression using per-token hide sets,
  argument prescan, multiline calls, empty arguments, variadic `__VA_ARGS__`,
  stringification and token pasting (including empty placemarkers).
* `#define`, `#undef`, `#include`, `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`,
  `#endif`, `#line`, `#error`, `#warning`, and `#pragma once`.
* Integer conditional expressions: `defined`, arithmetic, bitwise, shifts,
  comparisons, short-circuit logical operators and `?:`. Undefined identifiers
  become zero. Integers use 64-bit signed/unsigned evaluation. Character
  constants, hexadecimal/octal/binary numbers and U/L/LL suffixes are supported.
* `__FILE__`, `__LINE__` and `__COUNTER__`.
* Literal header names are opaque (including spaces and comment-like characters),
  while macro-generated include operands are expanded. Quoted includes search the
  including file's directory first; angle includes
  search only configured `-I` paths. Include guards work normally. Dependencies
  are recorded once per canonical path. Include failures carry source locations.
* Source-located errors and collected warnings; bounds on include depth,
  expansions, argument/expression nesting and output tokens.

This is **not yet a fully conforming replacement for a compiler's preprocessor**.
It does not discover compiler system-header paths or predefined target macros.
Trigraphs, universal-character identifiers, C++ raw strings, `__VA_OPT__`, GNU
comma-swallowing paste, `#include_next` and compiler-specific pragmas are not
implemented. Unknown directives fail; unknown pragmas produce a warning. Output
is token-preserving, space-separated text, not a source-format-preserving rewrite.
Multicharacter constants pack bytes left to right; signed right shifts are
arithmetic. Signed arithmetic wraps at 64 bits, except division overflow errors.

## Metaprogramming pipeline

Active ordinary source is macro-expanded, then rewritten, then rendered. Source
directives execute in order; inactive conditional groups invoke no extensions.
Generated tokens are expanded and rewritten, but never interpreted as directives.
No extension runs shell commands or loads native code implicitly.

### Token kernels

`#kernel NAME arguments...` expands its arguments, calls the named kernel, and
passes its result through the output pipeline. Three kernels are installed by
default:

```c
#define N 6
#kernel eval N * 7             // emits 42
#kernel stringify hello N     // emits "hello 6"
#kernel concat field _ count  // emits field_count
```

`eval` implements the integer-expression DSL (with literal operands, not unknown
identifiers); `stringify` quotes the canonical token spelling; `concat` requires
exactly one resulting preprocessing token. Applications can replace these kernels
or register their own, including parsers for token-based embedded DSLs.

### Rewriting

```c
#rewrite old_call ( ) => new_call ( 1 )
old_call();
```

`#rewrite pattern => replacement` installs an exact token-pattern rule. Rules are
tried in registration order, left to right. Replacements are **not rescanned** for
rewrites or macros, so cyclic rules cannot loop. A rule can erase tokens with an
empty replacement; its pattern must be nonempty. Rules can match across ordinary
source lines, but not across directive/include boundaries. This is token rewriting,
not AST patching or an e-graph engine.

### Native C++ API

```cpp
#include <MetaPP.h>

metapp::Preprocessor pp;
pp.define("N=7");
pp.register_kernel("double", [](const metapp::Tokens& args,
                                const metapp::Location&) {
    auto result = args;
    result.insert(result.end(), args.begin(), args.end());
    return result;
});
pp.register_directive("emit", [](metapp::Preprocessor&,
                                  const metapp::Tokens& args,
                                  const metapp::Location&) { return args; });
pp.add_rewrite(metapp::tokenize("old"), metapp::tokenize("new"));
auto output = pp.process("#kernel double N\n#emit old\n", "example.c");
// output: 7 7\nnew\n
```

`register_directive` receives unexpanded arguments, letting the handler decide
when to call `expand`. Core directive names cannot be overridden. Kernel arguments
are expanded before dispatch. Callbacks may throw `metapp::Error` with a supplied
source location. `tokenize` and `render` are also public for DSL parsers and
generators. The specialized `MetaPP-{Macro,Directive,Kernel,Rewriter}.h` headers
include the same API.

Macro definitions, kernels, directives and rewrite rules persist across calls,
including definitions made in source. Each call resets diagnostics, dependency
tracking, pragma-once state and counters. Failed calls do not roll back macro or
rule changes. Use a fresh instance for an independent translation unit. Instances
are not safe for concurrent use. Reentrant preprocessing calls are rejected without
resetting the active translation unit. Separate freshly constructed instances are
independent. Copies share registered callback objects (including mutable callback
state); do not copy or move an instance while it is processing. Callbacks may safely
replace their own registration; the old handler remains alive until it returns.

### C ABI / FFI

`MetaPP-C.h` exposes a versioned, opaque C interface with explicit create/destroy,
macro management, preprocessing, output/error accessors and foreign kernel
callbacks. C++ exceptions do not cross the ABI. See `tests/CAPI.c` for an example
including callback registration. Returned strings are borrowed; ownership and
lifetime are documented in the header.

## Scope of the existing language drafts

The pre-existing `.qi` interfaces and `kernels/Docstring.qs` are unfinished design
drafts, not executable modules. MetaPP does not currently implement the proposed
Qi/QS runtime, JSON/YAML/S-expression adapters, AST builders/patchers, general
term-rewriting/e-graphs or a shell executor. The native kernel/directive/FFI APIs
are the implemented integration points for such future modules. These drafts and
the pre-existing scaffold deletions have been left intact.
