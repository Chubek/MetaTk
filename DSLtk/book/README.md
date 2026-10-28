# DSLtk: The Definitive Guide

A 42-chapter, source-based book about **`DSLtk/DSLtk.hpp`**, with a dedicated
D comparison chapter and five executable case studies. Each chapter includes
a graph, tables, and pseudocode.

## Read

- **Illustrated book:** [`build/main.pdf`](build/main.pdf)
- **Markdown chapter index:** [`markdown/README.md`](markdown/README.md)
- **Single Markdown volume:** [`markdown/DSLtkBook.md`](markdown/DSLtkBook.md)
- **Verification evidence:** [`VERIFICATION.md`](VERIFICATION.md)
- **Sources and attribution:** [`PROVENANCE.md`](PROVENANCE.md)

## Build

Run these commands from `DSLtkBook`, beside the supplied `../DSLtk` directory:

```sh
make pdf          # assemble Markdown-authored chapters, run LuaLaTeX and BibTeX
make markdown     # readable chapter files and combined Markdown edition
make check        # structure, five C++ programs, expected-failure probes
make listings     # compile/run the complete C++ programs in chapter listings
make d-check      # compile/run the importing D companion
make verify       # all of the above, PDF diagnostics, verification report
```

From the parent directory, use `make -C DSLtkBook pdf`. The compatibility
entry point is `lua build.lua pdf` inside the book directory, or
`lua DSLtkBook/build.lua pdf` from its parent.
`make clean` removes TeX intermediates; `make distclean` removes the build
directory and PDF. Chapter sources, Markdown, and companion sources survive.

### Dependencies

- Python 3 and Pandoc (the generation script uses modern Pandoc's
  `--syntax-highlighting=none` option).
- TeX Live with LuaLaTeX, latexmk, BibTeX, fontspec, unicode-math, TikZ,
  tcolorbox, listings, adjustbox, xurl, biblatex, and cleveref.
- TeX Gyre Pagella/Heros and DejaVu Sans Mono fonts.
- A C++20 compiler; override with `make CXX=clang++ check` if desired.
- LDC for `d-check` (`DC=ldc2` by default).
- Lua 5.4 development files and pkg-config for the optional-Lua chapter
  listing audit. The five case studies do not require Lua.
- Poppler's `pdfinfo` and `pdftotext` for final PDF verification.

The book builds directly against the supplied header. Its Makefile does not
depend on the library's separate CMake installation workflow.

## Five case studies

| Chapters | Program | Demonstration |
|---|---|---|
| 38 | `projects/units/main.cpp` | Whole-spelling validation and dimensional arithmetic |
| 39 | `projects/query/main.cpp` | Query interpretation and semantics-preserving rewriting |
| 40 | `projects/config/main.cpp` | Preprocessing, PEG tokenization, transactional configuration |
| 41 | `projects/saturation/main.cpp` | Bounded equality saturation and independent evaluation |
| 42 | `projects/binary/main.cpp` | Explicit framing, endian conversion, short-transfer I/O |

`make examples` builds and runs all five. Individual targets are
`make build/units`, `make build/query`, and so on. Assertions are enabled in
the default build flags. Chapter listings include these source files directly.

## Editing and generation

- `main.ltx` includes exactly 42 chapter files, with explicit `.ltx` extensions.
- **Chapters 2–36:** edit their files in `chapters/` directly.
- **Chapters 1 and 37–42:** edit `essays/chNN.md`; `make assemble` regenerates
  their LaTeX. Chapter 37 additionally adapts the supplied D manual chapter.
- `scripts/atlas.json` supplies the generated chapters' diagrams, contract
  tables, operational pseudocode, and review questions.
- `macros.ltx` owns shared typesetting definitions. `references.bib` contains
  the bibliography. `make markdown` exports the current assembled chapters.
- Other essays are retained as supplementary editorial source material;
  the chapter files are authoritative for Chapters 2–36.

The Markdown edition retains TikZ diagram source in fenced blocks; the PDF
is the fully rendered illustrated edition. The listing verifier saves source,
compiler diagnostics, output, and content-keyed results under
`build/verification/`, so completed checks survive interruptions and are reused
when source, compiler, and header are unchanged.

Snapshot defects are discussed explicitly. The compile probes intentionally
expect rejection for ordinary closure predicate composition, duplicate-payload
Result queries, and the C++ task runner, while pattern-object dispatch must
compile and run. Application workarounds are labeled in their chapters.
