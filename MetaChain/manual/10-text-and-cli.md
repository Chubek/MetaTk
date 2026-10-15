# 10. Text interfaces and command-line reference

## Embedding commands without a process

```cpp
auto result = metachain::evaluate(
    "run", "const r0, 42\nprint r0\nhalt r0\n");
if (result) {
    // result.text == "42\nresult 42\n"
}
```

`evaluate(command, source, options)` accepts a command separately from its
payload. It does not split shell arguments or execute source strings as host
commands. `TextResult` contains text, binary bytes, diagnostics, and a boolean
success test. Text and bytes are separate to prevent accidental binary-to-text
conversion.

## Command table

| Command | Payload | Successful result |
|---|---|---|
| `parse` | grammar declarations, `%%`, source | committed tokens as text |
| `rewrite` | rules, `%%`, subject term | normal-form term text |
| `check` | MetaIL text | `ok` |
| `run` | MetaIL text | printed values, then `result VALUE` |
| `optimize` | MetaIL text | canonical optimized IL |
| `encode` | MetaIL text | MCIL binary bytes |
| `native` | MetaIL text | Linux x86-64 assembly |
| `build` | MetaIL text | static ELF executable bytes |
| `assemble` | x86-64 assembly | raw machine/data bytes |
| `object` | x86-64 assembly | ELF relocatable bytes |
| `link-assembly` | x86-64 assembly | assembled and linked executable |

Unknown commands fail. `decode` and multi-file `link` are driver operations:
the embedding equivalents are `Toolchain::decode`/`metail::emit` and
`Toolchain::link` respectively. `evaluate` accepts a text view and is not a
general binary I/O protocol.

## Driver invocation

```text
metachain COMMAND [INPUT|-] [-o OUTPUT] [--no-optimize] [--max-steps N]
metachain link OBJECT... -o OUTPUT
metachain --help
```

Omit input or use `-` to read standard input. The normal commands accept at
most one input file. `link` accepts multiple object paths. Options may follow
the command. Unknown options, missing values, duplicate output options, and
invalid step-limit integers are errors.

Binary commands require `-o`; the driver does not dump ELF or MCIL bytes into
an interactive terminal. Text commands write stdout unless `-o` is specified.
Diagnostics go to stderr. Exit status is 0 for success, 1 for failed processing
or I/O, and 2 for argument errors. A successful `run` command exits zero even
when the interpreted program's returned integer is nonzero: it is a result,
not the command driver's exit status.

The driver bounds ordinary input reads to 64 MiB. Linking uses the linker's
input-file and aggregate limits. It never infers execute permissions for an
output path. Use `chmod` or the embedding application's explicit permissions
policy after checking the generated artifact.

## Optimization and execution limits

`--no-optimize` prevents the compile facade from applying its default constant
propagation pass. It is useful for inspecting generated assembly and testing
the native backend against unfused arithmetic. The `optimize` command always
requests the optimization itself, even if the initial compile pass is disabled.

`--max-steps N` controls interpreter/native MetaIL instruction budgets. Zero
allows no instruction to run. It is not a wall-clock timeout and does not bound
the internal regex engine, filesystem latency, or native syscall blocking.

## Parsing example

```sh
build/metachain parse examples/list.mcg
```

The output names each committed token, followed by a quoted spelling. Grammar
literals use their literal text as the token name. This display is intended for
inspection, not as a stable binary-safe serialization of all possible byte
strings. For structured consumption, use `Toolchain::parse` and inspect events.

## Rewriting example

```sh
build/metachain rewrite examples/simplify.mcr
```

Rules precede a line containing only `%%`. The entire following payload is one
term, with leading/trailing whitespace permitted. A missing separator, invalid
rule, unbound RHS variable, cycle, or exhausted budget causes failure.

## Binary round-trip

```sh
build/metachain encode examples/factorial.mcil -o factorial.mcilb
build/metachain decode factorial.mcilb -o restored.mcil
build/metachain run restored.mcil
```

The binary format omits labels and source maps. Decoding emits numeric branch
targets and preserves behavior rather than original spelling/comments.

## Native artifact inspection

```sh
build/metachain native examples/factorial.mcil --no-optimize -o factorial.s
build/metachain object factorial.s -o factorial.o
build/metachain link factorial.o -o factorial
readelf -h -l factorial
```

`readelf` is an optional external inspection tool, not a runtime dependency.
Never execute an artifact merely because it has a valid ELF header. Validate
your provenance and deployment boundary first.
