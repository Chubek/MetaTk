# MetaML: A Meta-Language for Meta-Programming

An authored 42-chapter book covering the language, libraries, a complete
expression-language toolchain, C++/D/C embedding, and six chapters dedicated
to `DSLtk.hpp`. Chapter sources are `chapters/ch-1.me` through `ch-42.me`.
The current edition contains over 30,000 prose words, 125 listings, and 93
executable MetaML examples; its PDF is 96 pages with the tested groff setup.

## Build

From any working directory, invoke the script by its path:

```sh
./metaml-book/build.sh --build-luaroff --format all --check
./metaml-book/build.sh --no-build-luaroff --format pdf
./metaml-book/build.sh --pplua /path/to/pplua --format txt
./metaml-book/build.sh --install-luaroff --prefix "$HOME/.local"
```

Outputs default to `metaml-book/build/metaml-book.pdf`, `metaml-book.txt`,
`metaml-book.roff`, and `manifest.json`. `all` produces all three formats;
`roff` only preprocesses, without requiring groff. The text edition uses
textual graph and equation equivalents. The PDF uses vector pic diagrams.
The front reading map lists chapters; detailed page-numbered contents follow
the final chapter.

Run `build.sh --help` for output/build directory overrides, parallelism,
extra CMake arguments, and executable selection. Auto mode uses `--pplua`
or `$PPLUA`, then an existing local build, then PATH, and otherwise builds
the bundled source. `--build-luaroff` forces a local configure/build;
`--no-build-luaroff` prohibits compilation. Installation is opt-in, honors
`DESTDIR`, never invokes sudo, and continues using the local executable.

Dependencies:

* Bash and Python 3.
* groff with the me macros, pic, eqn, and gropdf for PDF; no TeX or Graphviz.
* To compile luaROFF: CMake, C++17, Lua 5.4 development headers/library, and
  the sol2 headers already under `luaROFF/third_party`.
* For `--check`: a MetaML CLI with the portable host bundle, defaulting to
  `../build/metaml-repl-cpp`. Select another with `--metaml` or `$METAML`.
  The book builder does not compile MetaML itself.

For example, from the DSLtk repository directory:

```sh
cmake -S . -B build -DMETAML_BUILD_REPL=ON -DBUILD_TESTING=OFF
cmake --build build --target metaml-repl-cpp --parallel 2
```

## Authoring

`index.lroff` supplies front matter; `macros/book.lua` owns titles, numbering,
and rofflet loading; `macros/book.me` configures groff's me package. The build
passes every chapter to pplua in numerical order before `macros/end.lroff`.
It does not rely on groff `.so` to preprocess nested Lua.

Custom implementations live in **`luaROFF/rofflets`**, including syntax
highlighting, graphs, grammar, pseudocode, equations, notes, symbols, and
UML-style dependency diagrams. The files in the outer `rofflets/` directory
are compatibility loaders. See `luaROFF/rofflets/README.md` for their API.
If distributing the nested luaROFF checkout separately, include its new
`rofflets/` directory along with the book: it is required source, not generated
output.

Use ordinary `.pp` paragraphs and `.sh 2 "Heading"` sections, with embedded
Lua for generated elements:

```text
.lua
book.code("metaml", [=[Test.equal "answer" (6 * 7) 42]=], "Arithmetic")
.endlua
```

Listings tagged `metaml` are complete independent programs with assertions.
`metaml-error:M0402` denotes a complete program expected to fail with that
code. `cpp`, `d`, and `c` mark standalone native programs; `*-fragment`,
`sh`, and `text` are explanatory listings. Pseudocode and descriptive EBNF
are explicitly labelled. Keep code lines within the printed measure.

## Checks

```sh
python3 metaml-book/scripts/check_book.py --structure-only
python3 metaml-book/scripts/check_book.py \
  --metaml build/metaml-repl-cpp \
  --pplua metaml-book/build/luaroff/pplua
python3 metaml-book/scripts/check_book.py --metaml build/tests/metaml-repl-d
python3 metaml-book/scripts/check_native.py
```

The checker verifies the chapter set, prose/section requirements, rofflet
implementations, and independent MetaML examples (including expected errors).
Rofflet smoke checks cover escaping, diagrams, equations, and invalid graph
endpoints. `--manifest FILE` writes chapter hashes and content statistics.
Native standalone listings can be extracted and compiled with the separate
`scripts/check_native.py` checker.
It checks eight standalone programs using C++20, C99, and LDC when available;
the C client links `build/libmetaml-c.a` (override with `--c-library`). Native
fragments remain explanatory. Both C++ and D interpreters have passed all
93 MetaML listings in this edition.

Builds reject luaROFF/groff diagnostics and render into a unique temporary
directory before publishing artifacts, preserving previous outputs on a
rendering failure. The manifest is deterministic source metadata, not a claim
that PDF bytes are identical across different groff versions or environments.
