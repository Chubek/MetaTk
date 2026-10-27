# 22 — Titles and sectioning

The LaTeX plugin stores `\title`, `\author`, and `\date` as document metadata.
`\maketitle` creates visible blocks from that metadata. Metadata arguments are
read as text, so arbitrary formatting or date macros inside them are not
evaluated like a general paragraph argument.

<!-- metatex-example {"format":"latex","contains":"Methods"} -->
```tex
\documentclass{article}
\title{A short report}
\author{Ada Example}
\date{October 2026}
\begin{document}
\maketitle
\section{Methods}
We measured the result.
\end{document}
```

`\section`, `\subsection`, and `\subsubsection` generate heading blocks and
maintain section counters for references. `\chapter` and `\part` currently
use the same top-level heading behavior as `\section`; they do not select a
book-specific page layout. `\paragraph` creates a deeper heading.

Optional heading arguments are consumed, but the engine does not maintain
separate short titles for running heads. Starred section commands are not
implemented as LaTeX's unnumbered variants. Heading numbers primarily support
reference values; visual numbering is renderer-specific.

`\tableofcontents` creates a placeholder block. The HTML renderer uses it to
list document headings. PDF does not implement a complete TeX table-of-contents
pass with page numbers. For predictable results, inspect both JSON structure
and the intended final renderer.

[Contents](README.md)
