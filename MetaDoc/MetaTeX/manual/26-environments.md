# 26 — Environments and theorems

LaTeX-format input uses `\begin{NAME}` and `\end{NAME}`. Each environment
establishes an engine group and invokes registered begin/end handlers. The
document environment is a supported container; unknown names produce a
diagnostic and matching remains important.

Define a zero-argument environment with token bodies:

<!-- metatex-example {"format":"latex","text":"Before: body :After"} -->
```tex
\newenvironment{framed}{Before: }{ :After}
\begin{framed}body\end{framed}
```

The bodies are scanned when declared and executed on entry/exit. Both
`\newenvironment` and `\renewenvironment` support up to nine parameters and an
optional first-argument default. Only the begin body may reference parameters.
Definitions obey group scope and reset at the next full compilation.

<!-- metatex-example {"format":"latex","text":"Left/Right: body!"} -->
```tex
\newenvironment{pair}[2][Left]{#1/#2: }{!}
\begin{pair}{Right}body\end{pair}
```

`\newtheorem{NAME}{LABEL}[PARENT]` supports parent-relative numbering;
`\newtheorem{NAME}[SHARED]{LABEL}` uses an existing counter. These two options
are mutually exclusive. `\newtheorem*` defines an unnumbered theorem. Instances
accept an optional title. The `proof` environment accepts an optional title and
ends with `QED`. These are semantic quote blocks rather than a theorem-style engine.

For native extensions, `registerEnvironment` takes two `Engine&` handlers.
Handlers can create semantic blocks or execute pre-scanned token lists. Keep
their effects consistent with group restoration, and report malformed input
through the document diagnostic API. See [chapter 37](37-native-plugins.md)
for the shared-library entry points.

[Contents](README.md)
