# 3. The command line and REPL

The REPL is a small language policy over DSLtk's generic session. On a terminal it displays `meaml> ` and a continuation prompt for incomplete input. Piped input has no prompts. Files run quietly except for explicit IO and diagnostics; `-e` evaluates a submission using REPL formatting.

```sh
build/metaml-repl-cpp --stdlib metalib -e 'List.sum [1, 2, 3]'
build/metaml-repl-cpp --stdlib metalib program.ml
build/metaml-repl-cpp --dump-ast program.ml
build/metaml-repl-cpp --stdlib metalib --list-primitives
```

Useful directives are `#type expr`, `#ast source`, `#trace expr`, `#load "file.ml"`, `#reload`, `#stack`, `#clear`, and `#quit`. The generic REPL also supplies `#help`. `#type` checks without evaluating; `#trace` evaluates and reports named reductions. `#load` remembers a path for subsequent reloads. The terminal wrapper checks the last loaded file for changes when reading subsequent input; this is not a continuously running background file watcher.

```text
meaml> inc x := x + 1
meaml> #type inc
meaml> #trace inc 41
```

History is stored beneath `$XDG_STATE_HOME/metaml`, or the user's local state directory. Readline support in the C++ wrapper is opt-in at compilation. `--no-history` and `--no-tty` make automated invocations predictable. `--no-stdlib` suppresses loading the portable prelude; it does not turn the REPL into a secure sandbox or uninstall host modules.

Expression results accumulate in the observable REPL stack. Definitions clear that stack, and evaluation failures clear it to avoid stale intermediate results. Embedding `eval` returns a value rather than relying on the REPL stack. Avoid using interactive transcripts as a substitute for scripts with explicit assertions.

**Sources:** [`repl/metaml.cpp`](../repl/metaml.cpp), [`repl/metaml.d`](../repl/metaml.d).

<!-- nav:begin -->
[Contents](README.md) · [Previous](02-building.md) · [Next](04-lexical-structure.md)
<!-- nav:end -->
