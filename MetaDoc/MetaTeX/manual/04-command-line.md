# 04 — Command-line reference

Batch syntax is `metatex [options] INPUT`, with one local input file.

| Option | Meaning |
|---|---|
| `-f`, `--format` | `plain`, `latex`, or `nekotex` |
| `-T`, `--target` | `html`, `pdf`, `json`, or `text` |
| `-o`, `--output` | Output filename |
| `--plugin NAME` | Load an additional plugin; repeatable |
| `-i`, `--interactive` | Enter the REPL, optionally after evaluating INPUT |
| `-h`, `--help` | Print usage |
| `--version` | Print the version |
| `--list-devices` | List supported target names |

Long format, target, and plugin options also accept `--option=value` syntax.
Batch target defaults to PDF. Without `-o`, the input extension is replaced by
the target extension (`.txt` for text). Format defaults to NekoTeX for `.nltx`
and LaTeX otherwise; a `.tex` suffix alone does not select `plain`.

No input file starts an interactive session. Interactive output defaults to
text; `-T` selects the device used when writing the accumulated document.

Exit status is 0 for successful batch compilation, 1 for document errors,
and 2 for exceptions such as invalid options, missing files, or plugin-loading
failures. Diagnostics go to stderr; status messages go to stdout. Batch
document errors prevent output writing. Do not treat the presence of an older
output file as evidence that the latest invocation succeeded.

[Contents](README.md)
