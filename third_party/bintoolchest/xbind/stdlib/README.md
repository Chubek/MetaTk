# XBind standard library

The standard library is a set of reusable helpers available to `.xbind` specs and
future target compilers. The C files in this directory are placeholders for these
modules:

| Module | Planned responsibility |
| --- | --- |
| `eval.c` | Execute/evaluate generated smoke-test fragments. |
| `exec.c` | Process and toolchain invocation helpers. |
| `io.c` | Portable file and path utilities. |
| `json.c` | JSON manifest and trace emission. |
| `peg.c` | Small parsing-expression helpers for specs/IR. |
| `regex.c` | Regex-backed declaration filters. |
| `rng.c` | Deterministic test-data generation. |
| `sexp.c` | S-expression parsing for type maps. |
| `toml.c` | TOML config import/export. |
| `typedef.c` | Type-map merge and lookup routines. |
| `yaml.c` | YAML config import/export. |

Keep these helpers dependency-light. Target runtimes should stay in `dombind/`;
this directory is for XBind's own driver and compiler support code.
