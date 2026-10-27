# MetaTeX project

Read the [44-chapter MetaTeX manual](manual/README.md) for installation,
authoring, macros, renderers, embedding, plugins, and scripting. Its marked
examples are exercised by the `manual-examples` CTest target.

The [subsystem guide](docs/SUBSYSTEMS.md) documents the command adapters, POSIX
IPC/network streams, FreeType/HarfBuzz font APIs, libHaru PDF device,
PostScript/ditroff/EPUB outputs, templates, and Lua binding.

**TeX82 / TeXScrape integration (new):** A genuine TeX82-to-DVI compiler is now
implemented in `truth/TeXScrape/Working/TeX.c`. For a dependency-free build,
use `cmake -S . -B build-tex82 -DMETATEX_TEXSCRAPE_ONLY=ON`,
`cmake --build build-tex82`, then `ctest --test-dir build-tex82`.
See [`truth/TeXScrape/README.md`](truth/TeXScrape/README.md) for examples,
font configuration, and known limits. This is a separate binary from the
existing MetaTeX C++ semantic engine and its LaTeX/NekoTeX plugins.

---

# MetaTeX — executable engine prototype

**Status: working, tested subset; NOT a full TeX82/LaTeX implementation.**

This package incorporates the supplied MetaTeX ZIP. Its source code was entirely
empty except for `manifests/tex-primitives.yaml`. The new C++20 implementation
can tokenize, expand a useful subset of TeX, execute external dialect plugins,
produce a semantic document IR, and render HTML, MathML, JSON, text, and PDF.

Full TeX compatibility, including TeX's page-building algorithm, complete
`\catcode` mutations, hyphenation, LuaTeX semantics, TeX font metric tables,
and arbitrary LaTeX packages, **is not implemented**. Do not use
this as a drop-in replacement for TeX Live or production publishing.

## Build

On Linux with CMake >= 3.18 and a C++20 compiler, using the workspace's dynalo,
replxx, DSLtk, MetaML/metalib and cpp-httplib sources, plus SQLite3/NNG development
libraries, libHaru sources, FreeType/HarfBuzz and Lua development files
(see [dependencies](docs/DEPENDENCIES.md) and [subsystems](docs/SUBSYSTEMS.md)):

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

PDF output uses the workspace's libHaru library with native semantic layout,
vector geometry, and limited base-font Unicode support. PangoCairo development
packages optionally enable the explicit `renderCairoPDF` API. No library download
is performed as part of the build.

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
`json`, `ps`, `troff-ir`, and `epub`. Format defaults to NekoTeX for `.nltx` or LaTeX otherwise. Output
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

- Incremental UTF-8-preserving lexer with scoped/global `\catcode` assignments
  for character codes 0–255, active-character macros, and category-preserving
  stored tokens, including category-aware `^^` input notation. Includes escape/control word, group, comment, math delimiter,
  parameter, superscript, subscript and space tokens.
- Expandable macros: `\def`, `\gdef`, `\edef`, `\xdef`, `\let`; scoped definitions; `#1`–`#9`
  undelimited and delimited arguments, literal parameter prefixes, and `#{`
  brace delimiters; `\csname` and basic branching (`\iftrue`,
  `\iffalse`, `\ifdefined`, `\ifnum`, `\ifdim`). Processing budget limits runaway macros.
- Shared native expansion scanner with `\noexpand`, `\expandafter`, expanded
  `\csname` names, `\string`, `\number`, and `\romannumeral`.
- Ordered `\aftergroup` token insertion after local state restoration, and
  LaTeX `\newcommand`/`\renewcommand`/`\providecommand` first-argument defaults.
- Scoped TeX82 integer registers `\count0`–`\count255`, `\countdef` aliases,
  `\the`, `\advance`, `\multiply`, `\divide`, and `\global` for these assignments
  and macro definitions. Arithmetic overflow and division by zero are diagnosed.
- Scoped dimension (`\dimen`), glue (`\skip`), and token-list (`\toks`) registers
  with typed aliases and global assignment. Dimensions use exact scaled points
  and nine physical units; glue includes finite and `fil`/`fill`/`filll` orders.
  Token lists retain unexpanded tokens, including when copied by `\the` into
  expanded definitions. Numeric register arithmetic is checked before storage.
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

- Full TeX82 primitive semantics, box/math-glue registers, glue/box layout,
  Knuth–Plass line breaking, page insertion/shipout, native DVI emission,
  and complete TeX input normalization including `\endlinechar`.
- True LaTeX kernel, .sty/.cls loading, NFSS, BibTeX/Biber, TikZ, tabular
  layout, proper floats, verbatim preserving comments, alignment mathematics,
  complete special-case token expansion, and
  arbitrary LaTeX package compatibility.
- LuaTeX semantics, MRuby/MicroPython bindings and the advanced `inter/*.midl` IR modules.
- Full support for the user's private third-party libraries. See
  `docs/DEPENDENCIES.md` for the integration status.

`manifests/tex-primitives.yaml` is retained **unchanged** as a reference
catalogue. It is *not* a proof that the listed primitives are implemented.

## Direct use of the library

```cpp
#include "MetaTeX.hpp"
metatex::Engine engine;
metatex::Document d = engine.compile("Hello, $x^2$.");
std::string html = metatex::renderHTML(d);
std::string json = metatex::renderJSON(d);
```

Build documents directly with the C++ API and DSLtk-powered native DSL:

```cpp
using namespace metatex::cpp;
auto doc = document(
    heading(1, "Results"),
    paragraph("Hello, ", bold("MetaTeX"), ". Formula: ", math("x^2"))
) | title("Native C++ report");
auto html = doc | renderAs(metatex::OutputFormat::HTML);
```

Link `metatex_core`; its CMake target supplies the include paths and C++20
requirement. `DocumentBuilder` supports imperative construction and dynamic
content. See the [C++ API guide](docs/CPP_API.md) and
[`examples/native.cpp`](examples/native.cpp).

See `docs/ARCHITECTURE.md`, `docs/COMPATIBILITY.md`, and `AGENTS.md` for
implementation constraints, extension contracts, and future tasks.
