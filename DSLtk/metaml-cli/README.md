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

An optional QBE native backend is available with `-DMETAML_ENABLE_QBE=ON` and
`--engine qbe` in both C++ CLIs and the D CLI; see [QBE.md](../QBE.md).

## Files and output

Source files and imports are searched in the current directory, repeated `-I`
directories, `METAML_PATH`, `METAML_LIBPATH`, and the selected standard library.
Imports also search relative to the importing file first. Both environment
variables contain colon-separated directories; empty components mean the current
directory. Files may be specified exactly, as `name` for `name.mml`, or as a
directory containing `mod.mml`. Quote paths with spaces in directives.

### Programs and interfaces

`.mml` files contain runnable code. A sibling `foo.mmli` declares the contract
for `foo.mml`; `.mlli` is accepted as an alternate spelling. Having both sibling
spellings is an error. Interfaces contain only type declarations, value
signatures (`name :: Type`), and module-type signatures, never definitions,
expressions, imports, or directives. This applies to the C++ and D frontends.

```metaml
{- counter.mmli -}
type Count := Int
increment :: Count -> Count
```

```metaml
{- counter.mml -}
increment n := n + 1
increment 41
```

Concrete types and module-type declarations in the interface are available to
the implementation without repeating them. Abstract declarations (`type Token`)
require a concrete implementation declaration. Repeated concrete declarations
must match. Every value signature requires an implementation with a compatible
type, including polymorphism. Extra implementation bindings are allowed and
remain visible; file interfaces are contracts, not opaque module boundaries.
Use the language's `:>` module ascription for hiding/abstraction.

Pairing applies to command-line loads, imports, reloads, and `--check`/`%check`.
The complete paired implementation is checked before its own code executes;
its imports are loaded first and may execute in normal (non-check) mode.
Passing an interface alone validates and loads its declarations without running
an implementation. `--ast` validates and shows the file and, for a program,
includes concrete declarations supplied by its companion.

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

## MAMMAL project manager

CMake also builds and installs **metaml-mammal** with `METAML_BUILD_REPL=ON`.
`MAMMAL` is an ordinary executable MetaML file, not JSON or a separate settings
language. It can use functions, conditionals, computed strings, and imports.
The configuration session has the built-in language environment and the
`Mammal` host primitives below; it does not automatically load metalib.
Project descriptions are trusted code, not a security sandbox.

```metaml
Mammal.project("hello")
Mammal.directory("src")
Mammal.module_path("src")
Mammal.main("src/main.mml")
Mammal.test("tests/main.mml")
Mammal.fixture("fixtures/input.txt", "input.txt")
Mammal.artifact("assets/message.txt", "share/message.txt")
Mammal.install("share/message.txt", "share/hello/message.txt")
```

| MetaML call | Meaning |
|---|---|
| `Mammal.project(name)` | Required project name |
| `Mammal.file(path)` | Add one `.mml`, `.mmli`, or `.mlli` source |
| `Mammal.directory(path)` | Recursively add sources in sorted order |
| `Mammal.module_path(path)` | Add an import search directory |
| `Mammal.main(path)` | Select and include the runnable entry point |
| `Mammal.test(path)` | Include a test program; nonzero exit means failure |
| `Mammal.build_dir(path)` | Select staging directory (default `_mammal`) |
| `Mammal.fixture(source, destination)` | Copy a project file into staging |
| `Mammal.artifact(source, destination)` | Publish a project file into staging |
| `Mammal.install(source, destination)` | Install a staged file relative to prefix |

All arguments are strings. File/directory/test/path/copy calls accumulate;
project/main/build_dir select their last supplied value. Main and tests are
automatically included, as are companion interfaces. Paths are project-relative
(installation sources are staging-relative, destinations prefix-relative).
Absolute paths, traversal outside these roots, symlink escapes, duplicate staging
destinations, and installation of undeclared/stale outputs are rejected. Fixtures
and artifacts currently copy individual files, not directories.

```sh
metaml-mammal init             # create MAMMAL without replacing an existing one
metaml-mammal show             # evaluate settings and list the resolved project
metaml-mammal check            # type-check sources; no staging or source execution
metaml-mammal build            # check, then stage sources, fixtures, and artifacts
metaml-mammal run -- arg1      # build, run main from the staging directory
metaml-mammal test             # build, run tests; stop at first failure
metaml-mammal --prefix /usr --destdir ./package install
metaml-mammal -C path/to/project show
```

Discovery searches from the current directory (or `-C`) upwards for `MAMMAL`.
Project commands use the sibling `metaml` executable, falling back to `PATH`;
`--metaml FILE` overrides it. Installation rebuilds first and copies only explicit
`Mammal.install` entries; it never implicitly installs fixtures. Build is currently
a checked, runnable source distribution, not a native compiler or dependency
download system. Staging is updated in place; unrelated existing files are not
deleted. Process launching currently requires POSIX; Windows reports an error.
Exit codes are 0 for success, 1 for project/I/O/language errors, 2 for invalid
arguments, or the failing child program's status.

Tests: `ctest --test-dir build -R metaml_mammal --output-on-failure`.

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
