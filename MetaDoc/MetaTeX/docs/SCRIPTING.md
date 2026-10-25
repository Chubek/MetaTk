# Launchers, interactive sessions, and scripting commands

## Launchers and REPL

`bin/mtex`, `bin/mlatex`, and `bin/nlatex` select plain MetaTeX, MetaLaTeX, and
NekoLaTeX respectively. They forward arguments unchanged to `metatex`:

```sh
bin/mtex                         # plain REPL
bin/mlatex -i                   # LaTeX REPL
bin/nlatex --interactive         # NekoTeX dialect REPL
bin/mlatex -i examples/hello.ltx # evaluate the file, then continue interactively
bin/mlatex -T pdf -o out.pdf examples/hello.ltx
```

The scripts look for `METATEX_BINARY`, a neighboring `metatex`, the source-tree
`../build/metatex`, then `metatex` on PATH. CMake also copies them into the build
directory and installs all three beside the executable. For an out-of-tree build:

```sh
METATEX_BINARY=/path/to/build/metatex bin/mtex
```

No input file, or `-i`/`--interactive`, selects the replxx-backed REPL. A terminal
gets editing, command completion, history, and a dialect-specific prompt. Pipes
use the same input API without prompts or history writes. History lives under
`$XDG_STATE_HOME/metatex/` or `$HOME/.local/state/metatex/`, separately per dialect.

Definitions and counters persist across submissions. Each submission executes
once; old submissions are not replayed. Balanced groups, math and environments
are collected across lines. Errors are reported and the session can continue;
the process returns nonzero if any submission failed. Definitions completed
before an error can remain in the session.

| REPL command | Action |
|---|---|
| `:help` | List commands |
| `:quit` / Ctrl-D | Exit |
| `:begin` … `:end` | Explicitly collect a multi-line submission |
| `:abort` | Discard pending input |
| `:show` | Render accumulated document text without reevaluation |
| `:load FILE` | Evaluate another file, retaining its definitions |
| `:write FILE` | Render the accumulated document with the selected `-T` device |
| `:reset` | Clear document/session definitions; keep startup plugins |

REPL display is text. `-T` controls `:write` and `-o` output; it defaults to text
in the REPL and PDF in batch mode. `-o` writes the accumulated document on a
successful REPL exit. End-of-input with an unfinished submission is an error.

## MetaML-backed commands

```tex
\mmlcommand{\greet}[1]{"Hello, " ^ String.upper arg1 ^ "!"}
\greet{world}

\mmlcommand{\bold}[1]{"\\textbf{" ^ arg1 ^ "}"}
\bold{Generated TeX}

\mmlcommandfile{\formula}{scripts/formula.mml}
\formula
```

Syntax: `\mmlcommand{\NAME}[ARITY]{BODY}` or
`\mmlcommandfile{\NAME}[ARITY]{PATH}`. Arity defaults to zero and is limited to
nine required TeX arguments. Each invocation receives `arg1` through `arg9` as
MetaML strings; `Process.args ()` exposes the argument list. Arguments are passed
as data, not interpolated into MetaML code. Definitions obey TeX group scoping
and can shadow existing commands.

Each invocation creates a fresh MetaML session, installs all implemented host
modules, loads the prelude, and evaluates the body. `IO.print`/`IO.println`
output is captured. A final String value is appended without quotes; other
non-Unit values use MetaML's textual representation. Unit adds nothing.
On success the generated UTF-8 is tokenized and inserted at the current position
in the TeX stream, so generated macros, formatting, math and commands execute
normally. On failure no captured output is inserted.

Inline bodies are ordinary TeX brace arguments: balanced braces, TeX comments,
and tokenized whitespace rules apply, and the body is not macro-expanded. For
arbitrary MetaML source (including `%` arithmetic, comments, or significant
whitespace in strings), use `\mmlcommandfile`. Files are read when the command is
defined; paths and MetaML imports resolve relative to the defining source file.

The full portable host bundle, File, Process, CBridge, FFI, HTTP, Threading,
Channels, Fiber, MessagePack, SQLite, NNG, IPC and Webserver installers are wired
in. All language modules are installed and available through `import`, including
array, sequence, stream, getopt, db, nosqldb, transpile, weblets, JSON-RPC, and
the service wrappers. Imports remain explicit beyond the standard prelude.
MetaML's normal `METAML_STDLIB` / `METAML_PREFIX` search is supported; installed
data is at `share/metaml/stdlib/cpp`, with the source tree as a build-time fallback.

MetaML output is capped at 4 MiB. Evaluator depth is 256, and the interpreter's
cancellation callback has a 30-second deadline. Native library calls retain
their own blocking and resource semantics. Host capabilities are enabled for
these user-authored commands. Sessions are closed and destroyed after invocation;
see [VALIDATION.md](VALIDATION.md) for an unresolved MetaML reference-cycle leak
found during sanitizer testing.

## Shebang-driven commands

```tex
\Extern{scripts/status.sh}
\Extern[fd=3,timeout=5000]{scripts/emit.py}

\externcommand{\report}[2][fd=1,timeout=5000]{scripts/report.py}
\report{first argument}{second argument}
```

`\Extern[OPTIONS]{PATH}` executes an executable script whose first two bytes are
`#!`. The kernel selects its interpreter from that shebang. No interpreter name
or shell command is guessed. `\externcommand{\NAME}[ARITY][OPTIONS]{PATH}` defines
a reusable, group-scoped command; invocation arguments become separate argv
entries. Script filenames resolve relative to the defining TeX file, including
nested inputs. The child inherits the process working directory and environment.

Options (comma-separated, without spaces):

- `fd=N`: capture child descriptor N (1–255), default 1 (stdout).
- `timeout=N`: milliseconds, 1–600000, default 30000.

Only the selected descriptor is emitted into the TeX stream. Unselected stderr
goes to the terminal; when selecting a descriptor other than stdout, ordinary
stdout is routed to the parent's stderr. Stdin is `/dev/null`. Captured bytes are
limited to 4 MiB and validated as UTF-8 by the engine. Nonzero exit status, signals,
timeouts, invalid output, and execution failures become document errors and
discard captured output. Failed/over-limit children are killed as a process group
and reaped. Arguments are never joined into a shell command.

The complete example is `examples/scripting.tex`:

```sh
build/mtex -T html -o scripting.html examples/scripting.tex
build/mtex -T pdf -o scripting.pdf examples/scripting.tex
```

## Embedding

All scripting commands are installed by `metatex::Engine`; no extra scripting
plugin is needed. `compile()` retains its batch reset behavior.
`compileFragment()` preserves session definitions and returns only the new
blocks while retaining metadata and references. `defineCommand()` installs a
group-local C++ handler, while `registerCommand()` remains the host/plugin API.
