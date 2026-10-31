# Building the book

The ZIP is self-contained for building the LaTeX book. The included PDF has 300 physical pages. The MetaML external `metalib` standard-library tree is not included and is not required by the examples.

## Requirements

- TeX Live or MiKTeX with LuaLaTeX, texlua, and the packages `geometry`, `fontenc`, `lmodern`, `microtype`, `xcolor`, `listings`, `titlesec`, `fancyhdr`, and `hyperref`.
- GNU Make is optional. `latexmk` is optional unless using the latexmk route.
- For checks: Python 3 and Poppler's `pdfinfo` (and `pdftoppm` for rendering).
- For native examples: a C++20 compiler with thread support. D examples need LDC (`ldc2`) or DMD (`dmd`) and Phobos. Do not disable assertions.

On Debian-family systems the relevant TeX collections are commonly `texlive-luatex`, `texlive-latex-recommended`, `texlive-latex-extra`, and `lmodern`; Poppler tools are packaged as `poppler-utils`. Package names can differ by distribution.

## Preferred build: texlua

From the extracted book directory:

```sh
texlua build.lua
```

This runs LuaLaTeX three times to settle hyperlinks, fails on TeX errors or reported box overflows, and writes `build/main.pdf` plus a convenience copy at `DSLtk-and-MetaML.pdf`. It does not use shell escape, network access, Python, or Pygments. The script can also be called with its absolute path; it changes into its own directory before building.

```sh
texlua build.lua check   # Python + pdfinfo: count pages and check logs
texlua build.lua clean   # remove regular files directly inside build/
```

Cleaning preserves sources and the delivered convenience PDF. To remove that PDF as well, delete it explicitly. Three passes are intentional; the first creates chapter labels used by the opening contents pages.

## Make and latexmk

```sh
make                    # same texlua build
make check              # build, then verify 300 pages and references
make latexmk            # use the included .latexmkrc
latexmk -lualatex main.tex
```

The latexmk route writes `build/main.pdf`; it does not refresh the root convenience copy. Use `texlua build.lua` for that copy. There is no bibliography or generated index to run. No Python generation is necessary for a normal build because all `.tex` chapters and listing `.inc` files are already present.

## Run all C++ workshops and case studies

```sh
make test               # 120 workshops + complete case acceptance tests
make test-cpp
make test-cases
```

The test helper chooses `g++`, then `clang++`, with `-std=c++20 -O0 -pthread`. It compiles the aggregate workshop runner and the two standalone case programs into `build/`. The formula tests include invalid input, zero rewrite budget, and 100 fixed-seed semantic-preservation fixtures. The policy test rejects ill-typed and behaviorally wrong candidates while retaining the active generation.

To run a single C++ example:

```sh
g++ -std=c++20 -O0 -pthread -Ivendor examples/cpp/ex01_03.cpp -o build/one
./build/one
```

Create `build/` first if it is absent. `examples/all_cpp.cpp` contains the same 120 bodies, with failure-reporting assertions, to avoid reparsing the headers 120 times.

## Run D examples

```sh
make test-d
ldc2 -Ivendor vendor/DSLtk.d vendor/MetaML.d examples/d/ex01_03.d -of=build/one-d
./build/one-d
```

The D test target compiles and runs all 120 workshops and both D case programs. It exits with an explicit NOT RUN message if neither `ldc2` nor `dmd` is available. D programs were source-reviewed but not compiler-tested in the authoring environment. A D module is compiled with the application; it is not literally a C++ header-only inclusion model.

## Case studies

```sh
g++ -std=c++20 -O0 -pthread -Ivendor case-studies/formula/main.cpp -o build/formula
printf '%s\n' '(mul (add 42 0) 1)' | ./build/formula
printf '%s\n' '(add 42 0)' | ./build/formula 0

g++ -std=c++20 -O0 -pthread -Ivendor case-studies/policy/main.cpp -o build/policy
./build/policy
```

Formula exit statuses: 0 normal form, 2 step limit, 1 invalid input or execution failure. Numeric atoms are unsigned 64-bit values and arithmetic is modulo 2^64. The sample reads the complete input into memory and uses recursion: add input-size/depth limits before using it on untrusted documents. The policy program is a synchronous service core; concurrency, transport and operating-system containment are application extensions.

## Editing and regeneration

Edit `chapters/*.tex` and listing files directly for ordinary LaTeX work. The optional editorial sources in `tools/content.json` and `tools/notes.json` can regenerate the authored chapter text and examples:

```sh
make regenerate
make test
make check
```

Regeneration overwrites `chapters/*.tex`, `frontmatter.tex`, `main.tex`, `SOURCE-MAP.md`, and the workshop example files. Preserve direct edits before running it. The two standalone case programs and the class/build files are maintained separately.

The supplied layout is twelve opening pages plus 24 twelve-page chapters. No blank pages are inserted to hit the target. Substantial edits can cause overflow into a new page; `make check` reports that instead of padding or cropping the result. Inspect the rendered page and adjust the content or layout deliberately.

## Visual review

```sh
pdftoppm -f 13 -l 16 -r 110 -png build/main.pdf build/review
```

Inspect chapter openings, long listings, paired workshop pages and the final case chapters. Review `build/main.log` for overfull boxes and unresolved references. `VALIDATION.md` records the checks performed on the shipped edition. `SOURCE-MAP.md` pins the four library inputs and links each workshop to an implementation entry point.
