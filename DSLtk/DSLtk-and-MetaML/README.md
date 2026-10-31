# DSLtk and MetaML: A Full Guide for the Working Programmer

A 300-page LaTeX guide based on the supplied C++ and D source snapshots.

- 24 chapters and 120 paired C++/D workshops.
- 240 workshop programs plus four complete case-study programs.
- Case study I: a validated formula optimizer with bounded term reduction and an e-graph alternative.
- Case study II: a reloadable policy core that checks candidates before publication.
- `DSLtk-and-MetaML.pdf`: compiled book.
- `main.tex`, `bookguide.cls`, `frontmatter.tex`, `chapters/`: editable LaTeX sources.
- `examples/`, `case-studies/`: runnable code and shared MetaML fixtures.
- `vendor/`: unmodified, canonically named copies of the four uploaded library sources.
- `SOURCE-MAP.md`: source hashes and implementation anchors.
- `BUILD.md`: dependencies and full build/test instructions.
- `VALIDATION.md`: actual validation evidence and limitations.

Build with `texlua build.lua` or `make`. Use `make test` for C++ examples and `make test-d` with an installed D compiler. The D code was source-reviewed but not compiler-tested during authoring.

The external MetaML `metalib` tree was not supplied and is not included. The guide uses the fresh-session core rather than assuming those modules exist.
