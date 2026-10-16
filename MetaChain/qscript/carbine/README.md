# Carbine

Carbine is a build system for QScript. Its orchestration (the dependency
graph, staleness, scheduling, command dispatch) is written in QScript, and its
toolchain (text, files, processes, parsing, emitters, archives, lint, fetch)
is C, exposed to QScript as integer-only natives under `carbine::`.

## Layout

- `source/*.qs` the engine, in QScript. `shell.qs` is the entry point.
  `depgraph.qs` is Kahn's topological sort and the dependency closure;
  `exec.qs` runs `carbine build`.
- `tooling/*.c` the C toolchain: model and plan (`model.c`), emitters for
  Ninja, Make, CMake and Meson (`emit.c`, `format.c`), execution (`exec.c`),
  archives (`archive.c`), lint, git description, HTTP and FTP fetch, the
  module loader, and the native table (`natives.c`).
- `tooling/spec.c` loads a Carbinefile with the QScript grammar in
  `grammar/carbine.grammar`. It is compiled only with `CARBINE_WITH_QSCRIPT`.
- `tooling/carbine_main.c` is the host: it parses the command line, registers
  the natives, and runs the engine.
- `include/` the public C API: `Carbine-API.h`, `Carbine-Module.h`
  (module ABI), `Carbine-Plugin.h` (`CB_MODULE` macro).
- `tests/qsi.py` a QScript interpreter for tests; `tests/engine_test.py`
  runs the engine against fake natives; `tests/unit_test.c` tests the C tools.
- `scripts/dogfooding.pl` builds the tooling, runs the tests and, given a
  QScript runtime, runs `carbine` on this project's own Carbinefile.
- `examples/` a Carbinefile and `hello.c15`, a QScript program.

## Build

Without QScript (builds the C tooling and runs the tests):

    perl scripts/dogfooding.pl

With QScript (builds the host too; `QSCRIPT_ROOT` is the QScript source tree):

    perl scripts/dogfooding.pl /path/to/qscript

## Carbinefile

    project NAME;
    import DIR;
    target lib|exe|program NAME {
        sources PATTERN...;   # globs; !PATTERN excludes
        deps NAME...;
        include DIR...;
        define NAME...;
        flags WORD...;
    }

`.c15` files are QScript programs (`program` targets, built with `qscript
build`). `.c` files are compiled with `cc` and archived with `ar`.

## Status

Verified here:

- The engine's logic: 20 tests of the QScript engine pass under `tests/qsi.py`,
  which interprets the real `source/*.qs` with fake natives. They cover graph
  order, cycles, staleness, selection, `-k` behaviour, and the gen, lint,
  check, version and dist commands.
- The C tooling compiles with `-Wall -Wextra` (and `-Werror` for the
  library), and the C unit tests pass: shell, make and ninja quoting, lint,
  URL parsing, and a tar archive listed by the system `tar`.
- `examples/hello.c15` runs under the interpreter and prints 55.

Not verified (the environment has no QScript runtime and no meson):

- The QScript runtime itself. The engine has only been run under the Python
  interpreter, so the `.qs` sources are checked against that interpreter's
  subset, not against the real compiler.
- `tooling/spec.c` and `tooling/carbine_main.c` have not been compiled against
  the QScript headers.
- The Carbinefile grammar has not been run through the QScript grammar engine.
- Emitted Ninja, Make, CMake and Meson files have not been checked with the real
  tools. Ninja and Make are available here and remain to be exercised on a
  generated project.
- The `carbine` target's include and library paths assume the QScript tree
  sits next to this one (`../qscript`).
