# MetaML CLI

The C++ and D frontends embed their corresponding MetaML runtime and metalib
host bundle. CMake builds `metaml-repl-cpp` and, with tests and a D compiler
enabled, `tests/metaml-repl-d`.

```sh
metaml-repl-cpp --stdlib ./metalib program.ml
metaml-repl-cpp -e '1 + 2'
metaml-repl-cpp -e 'answer := 42' -i
metaml-repl-cpp -- -filename.ml
```

Files run first, followed by `-e` expressions; a failed file stops batch
execution. `-i` enters the REPL after batch inputs. With no inputs, standard
input feeds the REPL. `--no-tty` suppresses prompts; `--no-history` disables
history persistence. `--no-stdlib` skips the language prelude, while host
primitives remain registered.

Exit status is 0 for success, 1 for evaluation/I/O failure, and 2 for invalid
options. File and AST diagnostics go to stderr. Interactive/piped REPL
diagnostics remain part of the REPL transcript on stdout.

Run `--help` for the complete option list. `scripts/check_cli.py --binary PATH`
checks argument handling, batch failure behavior, AST diagnostic routing,
interactive continuation and byte-preserving standard input.
