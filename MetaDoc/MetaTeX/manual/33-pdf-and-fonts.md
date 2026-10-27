# 33 — PDF output and fonts

The CLI's PDF path first tries the optional Cairo/Pango backend. If that
backend was not compiled in, it uses the built-in PDF 1.4 writer. Configure-time
messages report whether PangoCairo was found.

```sh
build/metatex -f latex -T pdf -o report.pdf report.tex
```

Pango/Cairo uses the platform font stack for shaping and Unicode glyphs. The
basic writer provides a small built-in font model with limited Unicode
coverage. Neither backend is a TeX82 box/glue/page-builder implementation, and
neither currently reproduces full TeX mathematical typesetting.

Paragraphs are wrapped and paginated by the selected backend. `\newpage` and
`\clearpage` in the LaTeX plugin create explicit page-break blocks. MetaVect
blocks preserve vector geometry and are fitted to the drawable area. Figures,
footnotes, and tables do not acquire full LaTeX float or insertion semantics.

For embedding, `renderPDF(document)` returns bytes from the basic writer.
`renderCairoPDF(document, path)` is the file-oriented shaped renderer and
returns false when unavailable. Choose deliberately rather than assuming the
in-memory helper automatically selects Cairo. Compare layout visually and
validate PDF structure; byte-for-byte output can vary with fonts and backend
versions even when semantic document tests pass.

[Contents](README.md)
