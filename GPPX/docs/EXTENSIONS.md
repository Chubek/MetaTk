# Extension-category APIs: from C to GPPX plugins

The GPPX 1.0.1 distribution has **two distinct layers**:

1. `include/GPPX.h` exposes the *versioned ABI* and main preprocessing operations. Existing ABI-1 shared plugins remain supported.
2. `include/GPPX-all.h` (or the individual `include/GPPX-*.h` headers) exposes category-specific utilities, adapters and registration helpers, with implementations in `GPPX.c`.

The source-tree root `GPPX.h` includes both layers. For installed builds, prefer `<GPPX-all.h>`.

The helper functions are compiled in the same shared library as the engine. `GPPX.c` is **not** a standalone implementation of the preprocessor: it uses `GPPX.cpp` for state, processing and plugin registration. Every caller-owned output returned by a helper must be released with `gppx_free()`.

## Directives

`gppx_directive_split("doit foo bar", &name, &args)` yields `doit` and `foo bar` as malloc-owned strings. `gppx_directive_valid_name()` validates the strict ASCII identifier convention. Register `gppx_directive_register(state, "doit", handler, userdata)` for a callback that processes the raw arguments and returns replacement text.

## Macros

`gppx_macro_define_spec(state, "MODE=release")` matches CLI `-D` notation. `-DMODE` yields `MODE=1`. `gppx_macro_process_text()` processes its argument as a full GPPX source, *including directives*, not an expression-only macro expansion. The normal engine macro-expansion API is also available to C++ callbacks.

## Syntax

`gppx_syntax_apply_profile(state, "template")` resets syntax to the mustache profile. `gppx_syntax_delimiters()` selectively overrides directive and macro markers. To add custom option keys, register a syntax callback with `gppx_syntax_register()` and invoke the option using `#syntax KEY VALUE` or `gppx_set_option()`.

## Readers and sinks

`gppx_reader_load_bytes()` reads a file into malloc-owned bytes with an independent length and extra terminating zero. It is usable for non-text binary payloads, unlike reader plugin callbacks whose return type is a C string.

`gppx_sink_write_file()` writes exact bytes, optionally appending. The reusable `gppx_sink_file_callback` accepts a live `GPPX_FileSinkConfig` struct as its userdata; the configured path takes precedence over the requested sink name. Do not point the config at a temporary variable that goes out of scope during processing.

## Embedded resources

`gppx_embed_hex`, `gppx_embed_c_array` and `gppx_embed_base64` accept a byte pointer plus explicit length, and output malloc-owned text. To register a new, independently named format:

```c
#include <GPPX-all.h>
GPPX_State *s = gppx_create();
if (!s) return 1;
if (gppx_embed_register(s, "hex2", gppx_embed_hex_callback, NULL)) return 1;
/* #embed "path/to/file" hex2 now uses the callback. */
gppx_destroy(s);
```

## Rewriters and matchers

`gppx_rewriter_replace_literal()` is a non-regex, nonrecursive replacement helper. `gppx_rewriter_literal_callback` accepts a live `GPPX_LiteralRewriter {needle, replacement}` struct and is suitable for `gppx_rewriter_register()`. This differs from `#rewrite`, which uses the core's regex engine.

`gppx_pmatch_glob` matches a **whole string** using `*` and `?`; `\\*` and `\\?` match those characters literally. `gppx_pmatch_prefix` checks a literal prefix. Both can be plugged directly into `#ifmatch` with the supplied callback adaptors.

## C callback contracts and safety

- Every replacement callback returning 0 must set `*output` to a malloc-owned string, or NULL for no replacement. Do not return static or stack strings.
- A callback may return nonzero to fail an operation. Pattern callbacks use a negative value for error, zero for no match, and positive for match.
- Reader plugins return **text**, not bytes with embedded NUL. Embed plugins are binary-safe on input but output text. `gppx_process()` uses NUL-terminated input/output.
- Plugin loading is native code execution; it is not a sandbox, and untrusted plugins must never be loaded.
- The plugin ABI version stays 1. Category utility additions do not alter the layout of `GPPX_HostAPI`.

Runnable C conformance checks live in `tests/test_extensions.c`; the original engine and Webber integration checks live in `tests/test.cpp`.
