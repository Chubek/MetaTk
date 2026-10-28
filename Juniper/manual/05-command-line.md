# 5. The command-line workflow

Choose commands by the artifact you need, not by guessing what an extension means.
The general form is `juniper COMMAND [FILE] [options]`. Only one input is accepted.

## Command reference

| Command | Result | IR validation? |
|---|---|---|
| `preprocess` | Expanded text and inclusion-marker comments | No |
| `check` | No artifact; success/failure status | Yes |
| `emit` | Complete canonical Juniper IR | Yes |
| `metail` | MetaIL source, without the backend | Yes |
| `optimize` | Canonical Juniper IR after MetaIL passes | Yes |
| `analyze` | MetaIL analysis JSON | Yes |
| `select` | Selected machine-IR JSON | Yes, then coverage checking |
| `schedule` | Selected and scheduled machine-IR JSON | Yes, then coverage/scheduling |

All artifact commands default to stdout; `-o FILE` writes a file and `-o -`
explicitly means stdout. Diagnostics go to stderr. `check` accepts no output
file or depfile. There is no `--in-place` mode.

## A traceable staged workflow

```sh
"$J" emit "$MANUAL_EXAMPLES/affine.jun" -o "$WORK/input.junir"
"$J" optimize "$WORK/input.junir" --no-preprocess \
  --passes ConstantFolding,AddZeroRight --rounds 2 -o "$WORK/optimized.junir"
"$J" analyze "$WORK/optimized.junir" --no-preprocess \
  --passes ControlFlowGraph -o "$WORK/analysis.json"
"$J" schedule "$WORK/optimized.junir" --no-preprocess \
  -o "$WORK/scheduled.json"
```

Preserving intermediate artifacts makes regressions easier to attribute. A shorter
equivalent style, when intermediate files are unnecessary, is:

```sh
"$J" schedule "$MANUAL_EXAMPLES/affine.jun" --optimize \
  --passes ConstantFolding,AddZeroRight --rounds 2 -o "$WORK/direct.json"
```

`--optimize` is accepted only by `select` and `schedule`. `--rounds` requires an
optimization operation; `--passes` requires optimization or analysis. `--passes`
does not add selection rules or change scheduler priorities.

## Streams and option boundaries

Stdin is supported only with `--no-preprocess`:

```sh
printf '%s\n' 'fn identity(x) { return x; }' | \
  "$J" emit - --no-preprocess -o "$WORK/identity.junir"
```

That input is expanded IR, not macro source. Supplying `-I` or `-D` with
`--no-preprocess` is a usage error. Use `--` before an input whose name begins
with `-`, placing any options before that delimiter:

```sh
"$J" check --no-preprocess -- ./-unusual-name.junir
```

## Exit statuses and safe automation

* **0:** success.
* **1:** invalid source, failed expansion, selection/scheduling failure, or runtime
  initialization failure.
* **2:** usage error, such as an unsupported flag combination.
* **3:** file I/O or input/output alias failure.

```sh
if "$J" check "$MANUAL_EXAMPLES/affine.jun"; then
  "$J" emit "$MANUAL_EXAMPLES/affine.jun" -o "$WORK/checked.junir"
else
  printf '%s\n' 'Juniper validation failed' >&2
fi
```

The CLI computes and validates an artifact before replacing its destination.
Each file replacement uses a same-directory temporary file and rename. Output
and depfile are separate replacements, not one multi-file transaction. Avoid
redirecting stdout onto the input: the shell would truncate it before Juniper
could perform its own alias checks.

[Previous](04-first-juniperfile.md) · [Index](README.md) · [Next](06-source-syntax.md)
