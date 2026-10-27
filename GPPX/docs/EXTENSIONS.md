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

## Style and layout utilities

These utilities use the existing syntax and rewriter categories; they do not
add fields to `GPPX_HostAPI` or create new engine callback categories.

`GPPX_Style` groups a macro style (`bare`, `dollar`, or `mustache`), line/block
comment markers, and the string-expansion flag. `gppx_style_apply()` applies
these settings to an engine. NULL string members leave settings alone;
`expand_strings=-1` leaves that flag alone. Use 0 or 1 to disable or enable it.
For example, `GPPX_Style style={"dollar",";","/*","*/",0};` configures explicit
macros and semicolon line comments. Profiles reset syntax, so apply the style
after the profile. Configuration is sequential, not transactional.

`gppx_layout_format(text, &layout, &output)` normalizes CRLF and CR to LF,
optionally expands tabs, and prefixes nonempty lines with spaces. The config
is `GPPX_Layout {tab_width, indent_spaces}`; zero tab width preserves tabs.
Tab stops count input bytes from the beginning of each original line, before
adding the indentation prefix. This is not Unicode display-width calculation.
Empty lines and the presence or absence of the final newline are preserved.
`gppx_layout_callback` adapts it to the line-rewriter slot; keep its configuration
alive until the engine is destroyed. Free standalone output with `gppx_free()`.

## UTF-8 text input

`gppx_utf8_next()` decodes one scalar from byte-counted input and advances the
provided offset only on success. It rejects truncated and overlong sequences,
surrogates, stray continuation bytes and scalars above U+10FFFF.
`gppx_utf8_validate()` checks an entire buffer and optionally reports the first
invalid sequence's offset (or the input size on success). Both accept U+0000
as a scalar; these functions do not pass data to the preprocessor.

`gppx_utf8_load()` additionally rejects embedded NUL bytes and removes one
leading UTF-8 BOM, yielding malloc-owned text suitable for `gppx_process()`.
Register `gppx_utf8_reader_callback` for an extension to validate files read
through the engine's reader interface. It needs no userdata. This validates
encoding; it does not normalize Unicode or extend the engine's ASCII macro
identifier rules.

## Qualified macro symbols

`gppx_metasym_name("module", "value", &name)` constructs `module_value`, with
strict ASCII identifier validation. NULL or empty namespaces leave the name
unqualified. `gppx_metasym_define()` and `gppx_metasym_undef()` operate on those
qualified names in the engine's ordinary macro table. This helper owns no
separate state and provides no hygienic naming: `a_b` + `c` and `a` + `b_c`
both produce `a_b_c`. Free returned names with `gppx_free()`.

`examples/mylang.cpp` demonstrates style configuration, qualified macros,
UTF-8 source loading, and a C++ custom directive. Build target `gppx_mylang`,
then run it without arguments for a demo or with a UTF-8 source filename.
## Function and profile helpers

`<GPPX-Function.h>` exposes `gppx_function_define(state, name, parameters, body)`
and `gppx_function_call(state, name, argc, argv, &output)`. Definition parameters
are comma-separated identifiers with an optional final `...`. Calls accept
separate textual arguments, preserving embedded commas without reparsing an
invocation string. They dispatch to either a function macro or a registered
native macro callback. Argument and replacement expansion follows the current
syntax. Release returned output with `gppx_free`. C++ callers can use
`Engine::call(name, arguments)`.

`<GPPX-Profile.h>` enumerates the four profiles with `gppx_profile_count()` and
`gppx_profile_name(index)`. `gppx_profile_apply` applies a base profile, optional
directive override and optional `GPPX_Style`. NULL fields preserve current
settings; application is sequential, not transactional.

`<GPPX-Jackscript.h>` provides the [Jackscript integration](JACKSCRIPT.md).
All three headers are installed and included by `<GPPX-all.h>`. These helpers
do not change the ABI 1 host function table.
