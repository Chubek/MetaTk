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

The bodies are scanned when the environment is declared and executed when
entered or exited. `\newenvironment` arguments are not currently supported;
the optional count must be zero. Registration is engine state rather than a
complete LaTeX environment-definition implementation, including its prefix
and grouping subtleties.

`\newtheorem{NAME}{LABEL}` installs a simple numbered heading and body
environment. It uses a named counter and does not implement shared numbering,
section-relative numbering, theorem styles, or automatic proof symbols.

For native extensions, `registerEnvironment` takes two `Engine&` handlers.
Handlers can create semantic blocks or execute pre-scanned token lists. Keep
their effects consistent with group restoration, and report malformed input
through the document diagnostic API. See [chapter 37](37-native-plugins.md)
for the shared-library entry points.

[Contents](README.md)
