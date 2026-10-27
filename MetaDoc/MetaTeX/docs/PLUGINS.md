# Native plugin contracts

The three plugins implement the following native document-authoring subset.
Use `-f latex`, `-f nekotex` (which also loads LaTeX), and `--plugin metavect`.
All emit the same `Document` IR for HTML, PDF, JSON, text and alternate devices.
The tests `plugin-contracts` and `plugin-integration` exercise real shared libraries,
including combinations of plugins, malformed input and engine reuse.

## LaTeX

### Definitions and environments

`\newcommand`, `\renewcommand`, and `\providecommand` accept zero through nine
arguments and an optional first-argument default. Their starred spellings are
accepted; the native engine does not distinguish TeX long/short macros. The name
must be one control sequence. Parameter references are checked before installation.

`\newenvironment` and `\renewenvironment` have the same argument-count/default
syntax. The begin body may use parameters; the end body may not. Definitions
are group-local, persist across REPL fragments at top level, and reset on the
next full compilation. End handlers are captured on entry, so redefining an
environment inside itself cannot change its pending closing action.
Begin/end bodies can wrap a nested environment (for example `\begin{quote}` /
`\end{quote}`) or a `\bgroup` / `\egroup` pair. End-body execution is bounded by
the outer environment's group; conditional branches cannot span execution frames.

```tex
\newenvironment{result}[2][Result]{\textbf{#1}: #2 }{ Done.}
\begin{result}{A measured value}Body text.\end{result}
\begin{result}[Observation]{Another value}More text.\end{result}
```

Unknown environments and mismatched endings are errors. `document`, `itemize`,
`enumerate`, `description`, `quote`, `quotation`, `abstract`, `center`, `figure`
and `table` are available. Quotes retain their type across paragraphs; enumerate
stores per-list ordinals, including nested lists. Description item labels are bold
prefixes. HTML uses indented blocks, not a nested-list tree. Centering, floats and
tabular layout remain approximations or unsupported.

### Headings, counters and theorems

Section commands through `\subparagraph` accept stars and optional short titles.
Stars suppress counter advancement; short titles are consumed but not retained.
Chapter/part share section-level behavior. Heading numbers are reference values,
not automatically printed prefixes. The current TOC includes starred headings too.

`\newcounter{name}[parent]` declares a counter and a `\thename` display command.
Stepping the parent resets descendants. `\setcounter`, `\addtocounter`,
`\stepcounter`, and `\refstepcounter` use checked integer arithmetic; the last
also sets the current label value. Display commands include `\arabic`,
`\roman`/`\Roman` (1–3999), and `\alph`/`\Alph` (1–26). These are native display
handlers, not expandable TeX integer registers. Existing permissive named-counter
set/step behavior remains available. Use native `\count` registers for expansion
and TeX register semantics.

```tex
\newtheorem{theorem}{Theorem}[section]
\newtheorem{lemma}[theorem]{Lemma}
\newtheorem*{remark}{Remark}
\section{Results}
\begin{theorem}[Main result]\label{main}A claim.\end{theorem}
\begin{lemma}A related claim.\end{lemma}
\begin{proof}[Argument]Reasoning.\end{proof}
```

Theorem bodies are quote blocks, with bold headings. Shared and parent-relative
numbering are mutually exclusive declaration options. Proofs finish with `QED`.
Figure and table environments bind `\caption` to their own counter. Captions
accept short titles and stars; starred captions do not advance the counter.
Labels work for numbered headings, theorems, equations, captions, exercises,
enumerated items and explicit `\refstepcounter`. References resolve at render
time; unresolved values remain `??`. `\pageref` is still a number-reference alias.

`equation`, `equation*`, `align`, `align*`, `gather`, `gather*` and `displaymath`
capture display math and check matching delimiters. Unstarred equation/align/gather
advance one equation counter per environment; `\label` is extracted into IR.
Alignment layout, per-row numbers, arbitrary nested math environments and full
AMS semantics are not implemented by this capture support.

### Literal source and package declarations

`\verb|literal % # \\ text|` and `\verb*|text|` read direct source literally until
a non-space ASCII delimiter, on one line. The star currently has the same visual
output. `verbatim` reads raw source until the exact `\end{verbatim}` marker.
Whitespace, comments and backslashes are preserved. Literal environments require
direct input, rather than a macro replacement or an already-tokenized argument.
Inline literal text in stored tokens can only preserve the tokens that survived
their original scan.

`\usepackage` and `\RequirePackage` share diagnostics: recognized names report
that only native subset behavior is available, unknown names report that they
are unimplemented. Neither loads `.sty` files or applies arbitrary options.
`\usepackage{metavect}` acknowledges an already-loaded plugin. `\textcolor`
reports that text color is unsupported while rendering its body. Class files,
NFSS, package loading, bibliography engines and complete LaTeX layout remain
outside this contract.

## NekoTeX

`\note`, `\tip`, `\warning`, `\important`, `\solution`, and `\summary` accept
`[title]{body}`. Each also has an environment form with an optional title.
`\callout[title]{body}` defaults to “Information”; `\component{name}{body}`
has the corresponding `\begin{component}{name}...\end{component}` form.

Bodies execute in a local group. Paragraphs retain their note role, nested notes
restore the outer role, and subsequent ordinary paragraphs remain ordinary.
HTML emits `data-kind`; JSON emits `role`. PDF and text include the title and body.

`\exercise[title]{body}` and the `exercise` environment number exercises and set
their reference value. `solution` is a separate titled note, with no automatic
exercise linkage or answer-hiding policy. `\concept` and `\meta` execute inline
formatting; `\kbd` and `\inlinecode` take literal brace arguments.

```tex
\begin{warning}[Check units]
First paragraph.

Second paragraph, still a warning.
\end{warning}
\codeblock[python]{  value = 100  # preserved
  print(value)
}
\begin{codeblock}{cpp}
int main() { return 0; }
\end{codeblock}
\codefile[python]{examples/sample.py}
```

Literal brace arguments preserve bytes after their opening brace and require
balanced literal `{`/`}` characters; use the environment or a file for unmatched
braces. The `codeblock` environment requires a language group (use `{}` for none),
then reads until the exact ending marker. Code files resolve relative to the
containing source, are limited to 4 MiB, and must be valid UTF-8 without NUL.
Language tags survive in JSON and HTML `data-language`; no syntax highlighter
runs. Native PDF uses Courier, retains indentation and expands tabs to four-column
stops. Very long code lines wrap to the available width.

## MetaVect

See [METAVECT.md](METAVECT.md) for scene syntax and all sizing/definition commands.
The plugin consumes versioned `.mvs` artifacts, retains typed immutable paths, and
shares them between repeated uses. It does not evaluate `.mvct` source programs.

## Extension compatibility

Existing `Engine` methods and the `metatex_register_plugin` entry point remain
available. New APIs are additive: scoped environment definition/query (including
`defineTokenEnvironment` for macro begin bodies and token-list endings), grouped
execution, literal readers, block contexts, code-block construction, named-counter
relationships, and reference values. `Block` appends role/language/list-number
metadata. Rebuild C++ consumers and plugins against the updated core and headers;
the plugin entry-point name is stable, not a promise of cross-version C++ binary ABI.
