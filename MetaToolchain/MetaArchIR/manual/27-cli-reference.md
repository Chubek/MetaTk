# Chapter 27 — CLI option reference

## Invocation

```text
marchir -T TARGET [options] INPUT
```

Exactly one input is accepted for compilation. Option values are separate command
arguments: use `--emit hex`, not `--emit=hex`. A single `-` denotes stdin or stdout
where an input/output path is expected. `--` stops option parsing.

| Option | Purpose |
| --- | --- |
| `-T`, `--target NAME` | Database architecture name; required except for target listing/help/version |
| `--db DIRECTORY` | Explicit MetaArchDB root |
| `--target-index N` | Target record within the specification; default 0 |
| `-o`, `--output FILE` | Output file or `-` |
| `--emit FORMAT` | `asm`, `bin`, `hex`, or `json`; default `asm` |
| `--check` | Complete the selected output path without writing |
| `--max-bytes N` | Input and output byte budgets; default 16777216 |
| `--list-targets` | Sorted architecture directory names |
| `--list-encoders` | Encoder names and parameter names |
| `--list-forms` | Index, mnemonic, encoder, operand count |
| `--list-registers` | Register name, width, encoding |
| `-h`, `--help` | Usage and options |
| `--version` | Current CLI language-version text |

Numeric CLI values are nonnegative decimal integers; their syntax is narrower
than the language's signed, base-prefixed immediate syntax.

## Valid combinations

Listing modes are mutually exclusive. They do not accept an input, an output
path or `--check`. Other than `--list-targets`, each listing requires a target.
Listings describe database records, not a prefiltered set of every instruction
valid for every operand or feature combination.

`--check` cannot be combined with an explicit output path. Its meaning depends
on `--emit`: `--emit asm --check` checks rendering, whereas `--emit bin --check`
also requires a flattenable image. A native statement can pass the former and
fail the latter.

For a file input with no output option, the extension becomes `.s`, `.bin`,
`.hex` or `.json`. For stdin, output defaults to stdout. Diagnostics go to stderr.
See the next chapter for pipeline behavior and status codes.

[Previous](26-json-images.md) · [Contents](README.md) · [Next](28-files-and-pipelines.md)
