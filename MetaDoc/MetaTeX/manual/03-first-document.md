# 03 — Your first document

Save the following as `hello.tex`:

<!-- metatex-example {"format":"plain","text":"Hello, MetaTeX!"} -->
```tex
Hello, \textbf{MetaTeX}!
```

Compile it to HTML, then PDF:

```sh
build/metatex -f plain -T html -o hello.html hello.tex
build/metatex -f plain -T pdf -o hello.pdf hello.tex
```

The native engine already provides basic styling, so this example needs no
plugin. `-f plain` makes that choice explicit. Open the HTML in a browser and
the PDF in a PDF viewer. Successful compilation prints the output path and
number of document blocks.

To inspect what the engine understood, also generate JSON:

```sh
build/metatex -f plain -T json -o hello.json hello.tex
```

Its paragraph contains text, bold, and text spans. This is a useful diagnostic
technique: if the JSON is correct but a visual result is wrong, investigate the
renderer rather than macro expansion.

For LaTeX-style sectioning, use `-f latex` and commands from
[chapter 22](22-titles-and-sections.md). Start from a small example when moving
an existing document into MetaTeX; package declarations do not install a full
LaTeX runtime. Keep input and output paths different: the CLI rejects attempts
to overwrite the source with its rendered result.

[Contents](README.md)
