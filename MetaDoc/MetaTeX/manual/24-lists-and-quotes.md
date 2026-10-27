# 24 — Lists and quotations

The LaTeX plugin provides `itemize` and `enumerate`. `\item` starts a list-item
block with the nesting level and an ordered/unordered flag.

<!-- metatex-example {"format":"latex","contains":"- First\n- Second"} -->
```tex
\begin{itemize}
\item First
\item Second
\end{itemize}
```

An optional item argument becomes a bold prefix. Nested lists increase the
stored level, which renderers use for indentation. The current renderer
presentation is simple: ordered-item markers need not form LaTeX's complete
numbering scheme, and HTML list items are styled blocks rather than a fully
general nested-list tree. An item outside a list is an error.

`quote` and `abstract` start quote-style blocks. Their end handlers finish the
current paragraph. `center` accepts a region and separates paragraphs, but it
does not currently enforce typographic centering. Environment names must
match when closing; a mismatched `\end` is diagnosed.

Use these structures for readable documents and consistent extraction into
JSON or text. Fine-grained list spacing, custom enumeration templates, and
all behavior associated with LaTeX's `enumitem` package are not implemented
merely because the package name is recognized by `\usepackage`.

[Contents](README.md)
