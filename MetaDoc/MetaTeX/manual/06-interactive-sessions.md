# 06 — Interactive sessions

Start a session with `build/mtex`, `build/mlatex`, or `build/nlatex`. The
replxx interface provides editing, history, and command completion on a
terminal. Pipe input uses the same evaluation path without terminal prompts.
Use `:begin` and `:end` to explicitly collect a multi-line submission.

| Command | Action |
|---|---|
| `:help` | Show REPL commands |
| `:show` | Display accumulated document text |
| `:load FILE` | Evaluate another file with current definitions |
| `:write FILE` | Render accumulated content using `-T` |
| `:abort` | Discard pending source |
| `:reset` | Clear document and definitions, preserving startup plugins |
| `:quit` | Exit; Ctrl-D also exits |

Definitions and integer registers persist between completed submissions.
Previous submissions are not re-executed. For example, defining `\count0=1`
once and advancing it in another submission updates that register once.
An error does not make a submission transactional: definitions completed
before the error can remain. Use `:reset` for a fresh session.

With `-o`, a successful exit writes the accumulated document. `-T` determines
that file's renderer; the interactive display itself remains text. History is
stored per dialect under `$XDG_STATE_HOME/metatex`, or under
`$HOME/.local/state/metatex` when the former is unset. Source collection is
heuristic, so explicit multi-line collection is useful around category-code
changes and unusual macro syntax.

[Contents](README.md)
