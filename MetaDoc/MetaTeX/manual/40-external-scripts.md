# 40 — External script commands

`\Extern[OPTIONS]{PATH}` runs an executable file beginning with a shebang.
`\externcommand{\NAME}[ARITY][OPTIONS]{PATH}` defines a reusable command whose
arguments become separate child-process arguments.

```tex
\Extern[fd=1,timeout=5000]{scripts/status.sh}
\externcommand{\report}[2][fd=1,timeout=5000]{scripts/report.py}
\report{first value}{second value}
```

Paths resolve relative to the defining TeX source, including nested input
files. The child inherits the process environment and working directory; it
does not automatically change into the script's directory. The kernel uses
the script's shebang to choose the interpreter. Arguments are not joined into
a shell command string.

The `fd` option selects descriptor 1–255 to capture, defaulting to stdout.
`timeout` is milliseconds, from 1 through 600000, defaulting to 30000.
Stdin is `/dev/null`. Unselected stderr remains visible; when a different
descriptor is captured, ordinary stdout is redirected to the parent's stderr.

Captured output is limited to 4 MiB and must be valid UTF-8. A failed child,
timeout, signal, or invalid output produces a document error and discards
captured output. Successful output becomes default-category TeX tokens at the
invocation point. These commands execute programs with the user's host
capabilities; process capture and time limits are not a sandbox.

[Contents](README.md)
