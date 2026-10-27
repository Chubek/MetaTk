# MetaTeX implementation status (27 October 2026)

This report distinguishes executable code from specifications and empty
scaffolding. The user-supplied `truth/TeXScrape` is the authoritative TeX82
basis; generated files and documentation are not substitutes for execution.

| Subsystem | Status | Verification |
|:--|:--|:--|
| TeX82 C compiler (`truth/TeXScrape/Working/TeX.c`) | **Implemented; independently builds** | Actual TeX input -> DVI with PlainTeX and Computer Modern TFM fonts |
| PlainTeX macro format | **Bootstraps** from bundled `PlainTeX.tex` | `.fmt` dumped, loaded; source in `truth/TeXScrape` |
| TeX82 macro, register, conditional, box, math, and page processing | **Working in tested cases** | `tests/test_engine.py` |
| DVI version 2 emission | **Working in tested cases** | DVI bytecode scanner and C++ DVI verifier |
| DVI parsing/validation (`device/DVI.cpp`) | **Implemented** | opcode bounds, page/stack, font and postamble verification |
| Stream, file, pipe, mmap, descriptor and memory input/output | **Implemented for documented POSIX interfaces** | `tests/test_streams.cpp` and linked from `core/IO.cpp` |
| Native MetaTeX C++ semantic engine | **Builds with workspace dependencies; still a subset** | Native core, plugins, CLI, and renderer tests; macro parameter scanning extended against TeXScrape's `macrocall`/`scantoks` |
| LaTeX and NekoTeX plugins | **Pre-existing subsets, not complete formats** | Original plugin source retained |
| Unicode/OpenType shaping, TeX82 extensions, e-TeX, XeTeX/LuaTeX | **Not implemented in TeXScrape** | TeX82 uses an 8-bit font model |
| Empty MetaTeX IR, network/IPC, fonts, alternate outputs | **Not implemented** | Empty scaffolds are retained rather than filled with invented functionality |
| Official TRIP compatibility / full LaTeX2e regression coverage | **Not established** | Needs original TRIP fixtures and broad compatibility suite |

## Fixed defects in TeXScrape's previously revised C port

- Moved clock initialization before writing TeX's `\time`, `\day`,
  `\month`, and `\year` registers: no more 1776 date in DVI output.
- Increased the original 40-byte filename limit to 1024 bytes, supporting
  long absolute source filenames instead of silently truncating them.
- Bootstrapping writes `.fmt` into a designated cache directory with temporary
  staging to avoid exposing incomplete formats; no generated format is
  committed to the source tree.

## Test conditions

Built with GCC using C11 for the translated engine and C++20 for helpers on
Linux x86-64. All tests pass in the dependency-free CMake configuration:

```sh
cmake -S . -B build-tex82 -DMETATEX_TEXSCRAPE_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-tex82 -j
ctest --test-dir build-tex82 --output-on-failure
```

TeXScrape's PlainTeX tests require `kpsewhich`, `cmr10.tfm`, `manfnt.tfm`,
and `hyphen.tex` (present in a normal TeX Live installation); INITEX tests
run without them. No TeX Live engine is used to compile test documents.

The regular MetaTeX C++ build requires the workspace-specific libraries
identified in `cmake/Dependencies.cmake`. They are available in the current
workspace, and the full Debug application now builds, including all three
plugins. The stream library is compiled with PIC for linkage into the native
shared library. Stand-alone TeXScrape results remain separate from native
MetaTeX verification.

## Native macro parameter scanning

`core/Engine.cpp` now implements literal parameter prefixes, consecutively
numbered parameters, multi-token delimiters, brace-protected delimiter
matching, single-group brace stripping, and TeX's special `#{` delimiter.
Replacement parameter references are validated before installing definitions.
Overlapping delimiters use linear-time matching; token processing retains its
existing budget. `tests/macros.cpp` checks exact input against semantic IR,
HTML, PDF text, malformed definitions, and a long overlapping delimiter case.
The reference is `macrocall` and `scantoks` in TeXScrape's working C source.
This does not complete native TeX macro semantics: long/outer prefixes,
complete primitive-meaning aliases, and full input normalization remain outstanding.

## Native expansion and integer registers

`core/Expansion.cpp` implements a shared expansion scanner used by execution
and expanded definitions. `\edef`/`\xdef` expand replacement text while retaining
unexpandable commands. Supported expansion primitives include `\noexpand`,
`\expandafter`, `\csname`, `\string`, `\number`, `\romannumeral`, `\the` (integer
registers), and the existing conditionals. Integer scanning handles expanded
input, repeated signs, decimal/octal/hexadecimal literals, and 8-bit character
constants. Expansion depth and processing budgets bound recursive input.

`core/Registers.cpp` implements 256 native integer registers with group-local
assignments, `\countdef`, arithmetic, and global assignments. Arithmetic uses
checked intermediates and preserves register contents on arithmetic overflow
or division by zero. Registers persist across REPL fragments and reset for a
new compilation. Dimension, glue, token-list, and box registers are still absent.

CTest targets `expansion-scanner` and `integer-registers` assert exact IR,
HTML, and PDF output as well as diagnostics and recovery. `native-tex82-oracle`
independently executes shared cases in native MetaTeX and TeXScrape INITEX,
without fonts or external formats, checking both against the expected result.
TeXScrape is a test oracle, not a runtime backend for the native implementation.

## Incremental category-code scanner

Native source and included files are scanned on demand using the current
category table. `\catcode` assignments accept character codes 0–255 and
categories 0–15, support local restoration and global overrides, and can be
queried by integer scanning or `\the`. Active-character definitions have
distinct identities from escaped control symbols. Stored macro tokens retain
their category and identity; delimiter comparisons distinguish letters from
other characters. Numeric lookahead also retains its original token category.

Conditional branches are consumed incrementally so assignments in a selected
branch affect subsequent source. Included files share current categories;
token-list execution preserves pre-scanned tokens. The scanner handles comment
line removal, control-word space skipping, and blank-line paragraph tokens.
The public `Lexer::scan` helper remains available with default categories.

`incremental-catcodes` checks IR/HTML/PDF output, local/global restoration,
active characters, macro token preservation, conditional branches, included
files, REPL state, malformed assignments, and category-sensitive delimiters.
The native/reference test now covers 30 shared cases. Full TeX input handling
is still incomplete: `\endlinechar`, configurable Unicode
categories above 255, and category-aware external script insertion remain
outstanding. Source NUL bytes are intentionally still rejected.

## Input notation, delayed tokens, and optional macro defaults

The native scanner now translates TeX's doubled-superscript notation before
category lookup, including inside control words. Both lowercase hexadecimal
and ASCII-shift forms are supported, along with repeated translations and
custom superscript characters. Translation consumes original bytes and retains
physical source locations; ignored comment tails are not translated. Encoded
8-bit characters are represented as UTF-8 in the native text model.

`\aftergroup` queues unexpanded tokens in declaration order and inserts them
after group-local state has been restored. The LaTeX command-definition plugin
now stores and uses optional first-argument defaults, including empty overrides,
invocation-time expansion, and scoped redefinitions. `source-features` asserts
exact source, document IR, HTML, PDF text, errors, and scanner positions.

## Manual and executable documentation

`manual/README.md` indexes 44 authored chapters covering the native application,
language, outputs, APIs, extension interfaces, verification, and actual limits.
`manual-examples` validates the chapter sequence and links and compiles 32 marked
examples into all four CLI formats. The manual, supporting guides, and this
status report are installed together under `share/doc/metatex`.

The native implementation is still incomplete. Typed TeX layout, remaining
register classes, full mathematical/page construction, general LaTeX package
loading, native DVI emission, and full compatibility validation have not been
completed by these additions or by documenting them.
