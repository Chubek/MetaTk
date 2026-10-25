# MetaTeX — executable engine prototype

**Status: working, tested subset; NOT a full TeX82/LaTeX implementation.**

This package incorporates the supplied MetaTeX ZIP. Its source code was entirely
empty except for `manifests/tex-primitives.yaml`. The new C++20 implementation
can tokenize, expand a useful subset of TeX, execute external dialect plugins,
produce a semantic document IR, and render HTML, MathML, JSON, text, and PDF.

Full TeX compatibility, including TeX's page-building algorithm, complete
`\catcode` mutations, hyphenation, Lua, font metric tables, macro delimited
parameters, and arbitrary LaTeX packages, **is not implemented**. Do not use
this as a drop-in replacement for TeX Live or production publishing.

## Build

On Linux with CMake >= 3.18 and a C++20 compiler, using the workspace's dynalo,
replxx, MetaML/metalib and cpp-httplib sources, plus SQLite3/NNG development
libraries (see [dependencies](docs/DEPENDENCIES.md)):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The build yields `metatex`, `libmetatex_core.so`, `metatex_latex.so`,
`metatex_nekotex.so`, and `metatex_metavect.so` in the build directory.
The sibling `../MetaVect/scene` library supplies vector scene interchange.
The CLI finds plugins beside
its executable (or under `METATEX_PLUGIN_PATH`). Plugin loading uses **dynalo**,
so the plugins must be built with an ABI-compatible compiler and core library.

PangoCairo development packages are optional. If `pkg-config pangocairo`
works when configuring, the PDF device uses Cairo, Pango, Fontconfig,
FreeType, and HarfBuzz indirectly for Unicode shaping. Otherwise a built-in,
valid PDF 1.4 writer is used, with limited fonts and Unicode glyph support.
No library download is performed as part of the build.

## Ready-to-run local build

`dist/linux-x86_64/` contains compiled Linux x86-64 binaries, packaged in the
same relative `bin/` and `lib/metatex/` layout used by CMake install.
These older binaries may predate current features; rebuild from source for the
MetaVect plugin and current engine fixes.

## Run

```sh
./build/metatex -f latex -T pdf -o hello.pdf examples/hello.ltx
./build/metatex -f latex -T html -o hello.html examples/hello.ltx
./build/metatex -f nekotex -T html -o modern.html examples/modern.nltx
./build/metatex -f latex -T json -o hello.json examples/hello.ltx
./build/metatex -f plain -T text -o plain.txt examples/plain.tex
./build/metatex --plugin metavect -f latex -T html -o vector.html examples/vector.ltx
./build/metatex --plugin metavect -f latex -T pdf -o vector.pdf examples/vector.ltx
./build/mtex                 # MetaTeX REPL (replxx)
./build/mlatex -i            # MetaLaTeX REPL
./build/nlatex --interactive # NekoLaTeX REPL
./build/mtex -T html -o scripting.html examples/scripting.tex
```

`-f` accepts `plain`, `latex`, `nekotex`; `-T` accepts `pdf`, `html`, `text`,
`json`. Format defaults to NekoTeX for `.nltx` or LaTeX otherwise. Output
is PDF by default. The source path and diagnostics are printed to the terminal;
undefined commands cause an error exit status, not silent acceptance.
Compilation errors preserve existing output files. Input and output cannot be
the same file. `--plugin NAME` adds a plugin to any dialect; plugin search paths
in `METATEX_PLUGIN_PATH` are colon-separated.

The three launchers are also under `bin/` and installed beside `metatex`.
No file or `-i`/`--interactive` starts an incremental REPL with editing,
completion, history, persistent definitions, and `:load`/`:write` commands.

`\mmlcommand{\name}[arity]{MetaML body}` and `\mmlcommandfile` create
MetaML-backed commands using the full installed metalib bundle.
`\Extern[fd=1]{script}` and `\externcommand` run shebang-selected scripts and
insert captured output into the TeX token stream. See [scripting](docs/SCRIPTING.md)
for syntax, argument passing, descriptor selection, and examples.

## MetaVect visualization

Use `\metavect[width=240]{SCENE}{CAPTION}` or
`\metavectfile[width=240]{diagram.mvs}{CAPTION}` with `--plugin metavect`.
Scenes become responsive inline SVG in HTML and native vector paths in both PDF
devices. JSON preserves the typed geometry. See [the plugin guide](docs/METAVECT.md).
MetaVect `.mvct` evaluation is unfinished; this integration consumes the working
versioned scene-artifact format, not arbitrary language programs.

## Implemented

- UTF-8-preserving lexer with TeX-like escape/control word, group, comment,
  math delimiter, parameter, superscript, subscript and space tokens.
- Expandable macros: `\def`, `\gdef`, `\let`; scoped definitions; `#1`–`#9`
  undelimited arguments; `\csname` and basic branching (`\iftrue`,
  `\iffalse`, `\ifdefined`, `\ifnum`). Processing budget limits runaway macros.
- Basic input inclusion, event-driven command handlers, document blocks,
  inline styles, headings, labels/references, lists and simple environments.
- External shared library plugins: **LaTeX** (document skeleton, sectioning,
  macros, math environments, links, figures as references, theorem definitions)
  and **NekoTeX** (semantic notes, callouts, tips, code blocks, exercises).
- TeX-style inline/display math to MathML, supporting fractions, roots,
  Greek symbols, selected operators and superscripts/subscripts.
- HTML with escaped content; JSON semantic IR; basic text; paginated PDF,
  including explicit page breaks. Optional PangoCairo Unicode PDF renderer.
- CMake, CTest, executable examples and Python integration checks.
- Bounded token scanning and nested execution, UTF-8 validation, scoped styles,
  checked counter arithmetic, and source-relative nested input with diagnostics.

## Not implemented yet

- Full TeX82 primitive semantics, register arithmetic, glue and boxes,
  Knuth–Plass line breaking, page insertion/shipout, DVI, PostScript, EPUB,
  TROFF IR, programmable active characters or mutable catcode tables.
- True LaTeX kernel, .sty/.cls loading, NFSS, BibTeX/Biber, TikZ, tabular
  layout, proper floats, verbatim preserving comments, alignment mathematics,
  special-case token expansion (`\edef` currently acts like `\def`), and
  arbitrary LaTeX package compatibility.
- Lua/MRuby/MicroPython bindings and the advanced `inter/*.midl` IR modules.
- Full support for the user's private third-party libraries. See
  `docs/DEPENDENCIES.md` for the integration status.

`manifests/tex-primitives.yaml` is retained **unchanged** as a reference
catalogue. It is *not* a proof that the listed primitives are implemented.

## Direct use of the library

```cpp
#include "MetaTeX.h"
metatex::Engine engine;
metatex::Document d = engine.compile("Hello, $x^2$.");
std::string html = metatex::renderHTML(d);
std::string json = metatex::renderJSON(d);
```

See `docs/ARCHITECTURE.md`, `docs/COMPATIBILITY.md`, and `AGENTS.md` for
implementation constraints, extension contracts, and future tasks.
