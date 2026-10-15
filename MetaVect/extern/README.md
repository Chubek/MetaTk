# Editor and document tooling

`extern/` mirrors the compiler lexer for editors and documents. It never
parses, evaluates, or emits graphics; the lexer (`frontend/Lexer.d`) is
authoritative and these files are best-effort mirrors.

## Layout

- `Vim/` — filetype detection (`.mv`, `.mvec`, `.mvct`), syntax
  highlighting, indentation, ftplugin defaults, and `:help` docs.
- `Sublime/` — `MetaVect.sublime-syntax` grammar with the same token
  coverage (keywords, booleans, conventional type names, numbers with
  invalid-number rules, strings with escape rules, nestable block comments).
- `LaTeX/` — documented package source (`metavect.dtx`), docstrip installer
  (`metavect.ins`), and l3build config. `metavect.sty` here and at
  `extern/metavect.sty` are the same generated file: run `latex
  metavect.ins` inside `LaTeX/` to reproduce it.
- `TeX/` — `MetaVect.tex` input snippet plus `Build.tex`, a compilable
  example (`pdflatex Build.tex`) showing figure inclusion and a `listings`
  sample. Replace `example-figure.pdf` with real backend output.
- `metavect.sty` — shipped generated package (figure inclusion, `MetaVect`
  `listings` language, `metavectcode` environment, `\MetaVectVersion`).

## Lexer alignment

Keywords, operators, delimiters, number shapes, string escapes (`\n \r \t
\" \\`), line comments (`--`), and nestable block comments (`{- -}`)
follow `frontend/Token.d` and `frontend/README.md`. Type names (`Int`,
`Float`, …) are conventional: the lexer sees identifiers. `()` is two
delimiters to the lexer (only the parser reads a unit literal).

Known best-effort gap: the `listings` package cannot nest `{- -}`
comments; Vim and Sublime nest them correctly.

## Version contract

`0.1.0` is spelled in `MetaVect.h`, `MetaVect.d`, the LaTeX package/date,
and `MetaVect.tex`. `tests/check-tex.py` fails on drift.

## Validation

- `tests/test-vim.vim` — highlight groups, nesting, escapes, bad numbers
  (batch Vim).
- `tests/test-vim-indent.vim` — indent levels, dedents, strings/comments
  ignored, unclosed `{-` (batch Vim).
- `tests/check-vim.py` — help tags, group coverage vs `Token.d`,
  ftplugin/indent options, ftdetect/Sublime extension agreement.
- `tests/check-sublime.py` — YAML shape, every token spelling covered,
  number/escape behavior.
- `tests/check-tex.py` — version sync, `.sty`/`.dtx` identity, docstrip
  reproduction, `pdflatex` build of `Build.tex`.
