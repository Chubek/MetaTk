# 23 — References and named counters

Place `\label{KEY}` after a heading to associate KEY with that heading's current
number. `\ref` creates a reference span; `\eqref` adds parentheses around it.
The LaTeX plugin's `\pageref` currently uses the same reference value rather
than computing a page number.

<!-- metatex-example {"format":"latex","contains":"See 1."} -->
```tex
\section{Results}\label{results}
See \ref{results}.
```

References live in the document's map and are resolved by rendering. They do
not require an `.aux` file or a second CLI invocation. Unresolved references
can render as `??`; review the output rather than assuming every missing
reference triggers a compilation error. Anchor and hyperlink behavior may
differ between HTML and PDF.

Named counters are a plugin-oriented facility:

<!-- metatex-example {"format":"latex","text":"5"} -->
```tex
\setcounter{attempts}{2}
\addtocounter{attempts}{3}
\arabic{attempts}
```

`\newcounter{NAME}[PARENT]` declares a counter with optional parent-reset
behavior and a `\theNAME` display command. `\refstepcounter` steps and sets the
current label value. `\roman`/`\Roman` support 1–3999; `\alph`/`\Alph` support
1–26. These display handlers are not expandable integer scanners.

`\stepcounter` adds one. Named-counter arithmetic is checked, but the values
are not the group-local `\count` register bank. Use `\countdef` when you need
TeX-style scoped integer storage. Numbered equations, theorem environments,
figure/table captions, enumerated items and NekoTeX exercises also establish
label values. `\pageref` still does not calculate physical page numbers.

[Contents](README.md)
