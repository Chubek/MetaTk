# MetaML command line

`MetaML.c` is the C++20 frontend built as **metaml**. It uses `MetaML.hpp`
and the vendored replxx line editor for history, syntax highlighting and Tab
completion of directives, identifiers, module members and file names.

```sh
cmake -S . -B build -DMETAML_BUILD_REPL=ON
cmake --build build --target metaml
build/metaml
build/metaml -l helpers.ml -e 'square 12'
build/metaml lesson.ml -- input.txt --verbose
build/metaml --check lesson.ml
```

Use `--help` for all options and `%help` or `%help type` for directive help.
The replxx source location can be overridden with `-DMETAML_REPLXX_DIR=DIR`.
The older `metaml-repl-cpp` executable retains its original CLI contract.

## Files and output

Source files and imports are searched in the current directory, repeated `-I`
directories, `METAML_PATH`, `METAML_LIBPATH`, and the selected standard library.
Imports also search relative to the importing file first. Both environment
variables contain colon-separated directories; empty components mean the current
directory. Files may be specified exactly, as `name` for `name.mml`, or as a
directory containing `mod.mml`. Quote paths with spaces in directives.

`--stdlib DIR` selects a library containing `prelude/mod.mml`. Otherwise the first
such directory in `METAML_LIBPATH` is selected, followed by MetaML's installed and
build-tree defaults. `--no-stdlib` uses just the built-in language environment.
Arguments following `--` are available as `Process.args ()`, without the script
name, including when the standard library is disabled.

Preloads run first, then the main file, then `-e`/`--type` actions in argument
order. Files and `-e` print their last value (including `()`); preloads suppress
automatic results but retain explicit language output. REPL expressions print
their type and value and are pushed onto the implicit stack. `-q` suppresses the
banner and automatic definition reports. Diagnostics, traces and timings go to
stderr. JSON diagnostics are newline-delimited objects on stderr.

`METAML_SINK=FILE` redirects language output, results and directive output to that
file, truncating it once at startup. Prompts remain on the terminal. An unusable
sink is an error. Exit status is 0 on success, 1 for language/I/O errors (including
errors recovered from during a REPL session), and 2 for invalid CLI arguments.

## Interactive sessions

Incomplete expressions, strings, comments and structures continue at the `..`
prompt. Ctrl-C cancels the current submission; Ctrl-D or `%quit` exits. Redirected
stdin uses the same submission handling without prompts or terminal escapes.

Before an interactive session, `~/.metamlrc` is read as REPL-style source and
directives unless `--no-init` is set. It is not read for batch/check/AST-only runs.
Persistent terminal history defaults to `$XDG_STATE_HOME/metaml/history`, or
`~/.local/state/metaml/history`. Override it with `--history-file`; `--no-history`
disables persistence but still permits in-session recall and `%history`.

Supported directives:

| Directive | Purpose |
|---|---|
| `%help [DIRECTIVE]` | List commands or explain one |
| `%type EXPR` | Infer a type without evaluation |
| `%load FILE`, `%reload` | Evaluate a file, or reevaluate the last loaded file |
| `%ast SOURCE` | Display the parsed AST |
| `%trace EXPR` | Evaluate once with the interpreter's reduction/call trace |
| `%time EXPR` | Evaluate once and report elapsed time |
| `%stack`, `%clear` | Display or clear the implicit stack |
| `%bindings [PREFIX]` | List visible values and constructors with their types |
| `%inspect NAME` | Show type, materialized value and available source metadata |
| `%check FILE` | Check a file and imports in an isolated type environment |
| `%history` | Number previous submissions in the current session |
| `%reset` | Restore libraries and clear definitions, stack and last-loaded file |
| `%paths [DIR]` | Show search paths or append a directory |
| `%which FILE` | Resolve a source/module path |
| `%modules` | List loaded files |
| `%pwd`, `%cd DIR` | Show/change the working directory |
| `%quit` | Exit |

`%check` clones the current type environment, including module/functor ASTs and
type graphs. `--check` builds the library environment without evaluating its
prelude or imports. Neither executes checked code. Source-level directives are
rejected in checked files. `%inspect` does not force unevaluated module bodies;
such values are reported as not materialized. `%reset` preserves history, program
arguments and configured search directories, and does not replay `.metamlrc`.

Tests: `ctest --test-dir build -R metaml_replxx --output-on-failure`.

## Legacy C++ and D frontends

The earlier C++ and D frontends embed their corresponding MetaML runtime and
metalib host bundle. CMake builds `metaml-repl-cpp` and, with tests and a D
compiler enabled, `tests/metaml-repl-d`.

```sh
metaml-repl-cpp --stdlib ./metalib program.mml
metaml-repl-cpp -e '1 + 2'
metaml-repl-cpp -e 'answer := 42' -i
metaml-repl-cpp -- -filename.mml
```

In these frontends, files run first, followed by `-e` expressions; a failed file
stops batch execution. `-i` enters the REPL after batch inputs. With no inputs,
standard input feeds the REPL. `--no-tty` suppresses prompts; `--no-history`
disables history persistence. `--no-stdlib` skips the language prelude, while
host primitives remain registered. Here `--` introduces file names rather than
program arguments, and directives use `#`.

Exit status is 0 for success, 1 for evaluation/I/O failure, and 2 for invalid
options. File and AST diagnostics go to stderr. Interactive/piped REPL
diagnostics remain part of the REPL transcript on stdout.

Run `--help` for the complete legacy option list.
`scripts/check_cli.py --binary PATH` checks argument handling, batch failure
behavior, AST diagnostic routing, interactive continuation and byte-preserving
standard input.
