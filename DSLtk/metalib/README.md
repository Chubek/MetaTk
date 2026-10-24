# metalib layers

`MetaML.hpp` and `MetaML.d` own parsing, checking, evaluation, sessions and core
primitives. `metalib` adds language modules and host services. `metaml-cli`
connects a session to command-line arguments, files, standard streams and a REPL.

## Language library

`prelude/mod.ml` imports the portable modules (`core`, `int`, `float`, `string`,
`list`, `option`, `result`, `io`). Load it with `Session.load_stdlib(directory)`.
Each module's `test.ml` is exercised by `scripts/run_tests.py`.

## Host bundles

Include `metalib/cpp/Metalib.h` or import `metalib.d.Metalib`. Install host
bundles before loading the language library:

| C++ / D snake-case API | Contents | Dependencies |
| --- | --- | --- |
| `install_foundation` | String, List, Hash, Bridge, Buffer, Table, Graph | Core runtime |
| `install_data` | URI, JSON, TOML, YAML, SExpr | Foundation |
| `install_language` | AST builders/walkers, lexer/parser bridges, Frontend, Type, Module | Data |
| `install_io` | IO, Loader | Foundation |
| `install_all` | All above, plus Metalib.version | All layers |

D also provides `installFoundation`, `installData`, `installLanguage`,
`installIOBundle`, and `installAll`. Dependencies are installed automatically.
Bundle composition skips modules whose entry-point primitive is already
registered, preserving opaque types and existing handles. Install complete
modules rather than registering just their entry points manually.

Installation does not grant primitive capabilities; embedders select them with
the session allowlist. The CLI enables the portable host bundle. Platform/native
modules such as File, Process, CBridge and FFI have explicit individual installers.
The C embedding adapter is in `c/`; C++ `.c` files are header implementation
fragments, not standalone C translation units.

`IO.read_all` preserves input bytes, including CRLF, NUL and a missing final
newline. `IO.read_lines` splits lines and strips their line endings.

## Verification

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

When a D compiler is present, this includes mirrored runtime tests, CLI stream
contracts and cross-dialect fixture/AST/primitive-manifest comparisons.
