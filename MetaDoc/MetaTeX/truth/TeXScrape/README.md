# TeXScrape: a native TeX82 -> DVI compiler

`Working/TeX.c` is the **working, compilable C engine**. It is derived from
`Revised/TeX.c`, itself a corrected port of `Artifacts/TeX.c`. The source of
truth remains the repository's original `TeXSource/TeX.p` (tangled TeX82
Pascal-H), `TeXSource/TeX.pool`, the 7 MB structured
`TeXSource/TeX.yaml` (CodeYAML), and `PlainTeX/PlainTeX.tex`. We did **not**
replace Knuth's engine with a simplified macro parser or an external TeX binary.

The old `Artifacts/TeX.c` is a *generated transliteration* with unfinished
routines. Do not compile it as the engine or overwrite the corrected `Working`
source with it. `Working/TeX.c` is a 29k-line native C program with the original
TeX82 interpreter, node memory, hyphenation, page builder, TFM loading, format
dumping/loading, and DVI writer. It embeds `TeX.pool` as checked string data,
so you do not need an external pool file.

## Build independently

```sh
cmake -S truth/TeXScrape -B /tmp/texscrape-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/texscrape-build -j
ctest --test-dir /tmp/texscrape-build --output-on-failure
```

Or build **only this subsystem** from the enclosing MetaTeX root (no DSLtk,
MetaVect, S7, replxx, or any other project dependency):

```sh
cmake -S . -B /tmp/metatex-tex82 -DMETATEX_TEXSCRAPE_ONLY=ON
cmake --build /tmp/metatex-tex82 -j
ctest --test-dir /tmp/metatex-tex82 --output-on-failure
```

This produces `bin/texscrape` and `bin/texscrape-dvi`. The former produces
actual `.dvi` and `.log` files; the latter independently checks DVI structure,
font definitions, page chain, stack behavior, opcode bounds, and postamble.

## Bootstrap and compile Plain TeX

TeXScrape handles files and fonts through the traditional colon-separated
`TEXINPUTS`, `TEXFONTS`, and `TEXFORMATS` paths. Unlike TeX Live, it does
not automatically search a `texmf` tree. The helper discovers the needed
external TFM fonts and `hyphen.tex` through `kpsewhich` if installed, without
launching a different TeX engine. An existing `TEXFONTS` environment variable
can also supply the font locations manually.

```sh
export TEXSCRAPE_TEX_ENGINE=/tmp/texscrape-build/bin/texscrape
export TEXSCRAPE_FORMAT_DIR=/tmp/texscrape-formats
sh truth/TeXScrape/PlainTeX/Bootstrapped.sh --init-only
sh truth/TeXScrape/PlainTeX/Bootstrapped.sh examples/plain.tex
/tmp/texscrape-build/bin/texscrape-dvi --json plain.dvi
```

The helper writes generated formats to a cache directory (`~/.cache/texscrape`
by default), never into the source of truth. The output is standard DVI version
2; with a separate `dvipdfmx` or `dvips` tool it can be converted to PDF or PS.
These external programs are **not** part of the engine and are not required
for its DVI output.

You can also use the TeX82 command convention directly:

```sh
TEXINPUTS=/path/to/plain TEXFONTS=/path/to/tfm \
  /tmp/texscrape-build/bin/texscrape '\input PlainTeX \dump'
TEXFORMATS=. TEXFONTS=/path/to/tfm \
  /tmp/texscrape-build/bin/texscrape '&PlainTeX' document.tex
```

## Architecture and limits

The engine has TeX82's **8-bit** input and font model. Plain TeX is actually
bootstrapped from the archived source, including the macro definitions and
hyphenation pattern input. It does not implement modern Unicode engines,
e-TeX primitives, LuaTeX, XeTeX, ConTeXt, or arbitrary LaTeX2e packages.
This native TeX82 engine is a **separate toolchain target**, not an alias for
MetaTeX's existing limited `Engine` class or LaTeX/NekoTeX plugins; converting
the latter's typed semantic IR to TeX82 bytecode would require an explicit
adapter and would lose their modern extensions.

`tests/test_engine.py` tests INITEX, format dumping/loading, expansion,
conditionals, registers, math, line breaking, nested input, boxes, multiple
pages, error handling and actual DVI bytecode validation. Font-dependent cases
are skipped only when `cmr10.tfm`, `manfnt.tfm` or `hyphen.tex` cannot be found
with `kpsewhich`; INITEX still runs. `tests/test_streams.cpp` tests the
standalone MetaTeX file, memory-mapped, stream, descriptor and memory I/O.
Passing these tests is **not** proof of passing the official TeX TRIP test.
