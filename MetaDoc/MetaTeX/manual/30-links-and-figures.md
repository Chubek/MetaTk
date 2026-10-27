# 30 — Links, figures, and annotations

The LaTeX plugin provides `\href{URL}{LABEL}` and `\url{URL}`. Both create link
spans; arguments are read as text rather than fully expanded rich-content
trees. HTML emits links, while text extraction keeps the visible label.

<!-- metatex-example {"format":"latex","text":"Visit the project."} -->
```tex
Visit \href{https://example.org/}{the project}.
```

`\includegraphics[OPTIONS]{PATH}` creates an image reference. HTML can point to
the resource, but the native PDF backends do not provide a general image-file
embedding pipeline. Keep referenced files available to the browser when
distributing HTML. The current options are consumed rather than interpreted
as a complete graphics sizing/cropping language.

`figure` and `table` environments provide paragraph boundaries, not float
placement or tabular layout. `\caption` inserts a textual Figure label.
`\footnote` inserts an inline annotation instead of reserving page-footnote
space. `\cite` displays bracketed citation text without bibliography lookup.

For graphics that must remain vector geometry in both HTML and PDF, use
[MetaVect](31-vector-graphics.md). For document navigation, inspect each target:
HTML hyperlink markup and PDF presentation do not imply identical link
annotations. A semantic reference is useful even when a backend has only
textual support for its destination.

[Contents](README.md)
