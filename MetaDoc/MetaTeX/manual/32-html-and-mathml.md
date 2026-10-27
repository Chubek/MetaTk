# 32 — HTML and MathML output

Select `-T html` to emit a complete UTF-8 HTML document. The renderer includes
basic styling and escapes ordinary text and attribute content. It translates
semantic spans into markup such as strong emphasis, code, and links rather
than interpreting arbitrary text as HTML.

```sh
build/metatex -f latex -T html -o report.html report.tex
```

Headings receive document identifiers. Note blocks become asides, quotations
become blockquotes, and vector blocks embed SVG. A table-of-contents block
can enumerate the document's headings. Explicit page-break blocks use a CSS
break hint, whose effect depends on browser printing behavior.

Math spans are converted to MathML by the native math parser. Test in the
browser used by the audience: browser MathML support and installed fonts can
change visual layout. This HTML path does not determine the PDF backend's
mathematical rendering.

The renderer supplies a practical default stylesheet, not a theme/plugin API
for every layout property. For a custom site, either post-process the emitted
HTML or implement a renderer over `Document`/JSON. Keep escaped content
escaped when doing so. Image references remain external resources; an HTML
file is not necessarily a self-contained archive of every referenced asset.

[Contents](README.md)
