# Chapter 7 — Command-line reference

[Previous](06-first-programs.md) · [Contents](README.md) · [Next](08-values-and-arithmetic.md)

## Machinery and VMWeave

```text
metavm-run MODULE [--threaded]
vmweave compile INPUT OUTPUT.mvmb
vmweave emit-c INPUT OUTPUT.c SYMBOL
vmweave-definition-demo [OUTPUT.c]
closure-vm-demo
```

`metavm-run` loads assembly or Machinery binary, finds a function named `main`,
and invokes it with no arguments and default limits. It prints scalar results
or an object-kind/length summary, not arbitrary object contents. It has no
general argument-passing, host-registration, or limit-setting flags.

`vmweave` accepts Machinery assembly/binary, **not MetaML source**. Binary and
C outputs use staged atomic-write APIs. A malformed input or invalid factory
symbol preserves an existing destination. Factory names must satisfy the
emitter's identifier/reserved-name restrictions; use an application-specific
prefix. The demos are fixed host examples, not arbitrary guest-format runners.

The two main tools return 0 on success, 1 on load/build/runtime failure, and 2
on usage error. Do not assume all tools in the project have identical I/O codes.

## SSAVM

```text
ssavm-run [--max-steps N] [--max-array-elements N]
          [--max-total-elements N] [--max-arrays N] [--check]
          [--max-analysis-states N] [--max-analysis-cells N]
          [--max-analysis-work N] [--] FILE.ssa [INTEGER_ARGUMENT ...]
```

Options precede the filename. `--` ends option parsing. Following integers are
guest arguments, including negative values. Execution prints an integer or
`array[length]`. `--check` parses/analyzes without execution, prints `verified`,
and accepts no guest arguments. Analysis-limit flags affect check mode only.
Limits are unsigned decimal; zero is a real ceiling. Statuses are 0 success,
1 input/parse/runtime/analysis failure, and 2 usage/option failure.

## DVI

```text
dvi-run [--metrics FONT_WIDTHS.tsv] [--threaded] FILE.dvi
dvi-run --help
```

Metrics rows contain decimal `FONT CHARACTER WIDTH` with optional `#` comments;
width is an already-scaled signed i32 DVI distance. No font lookup is performed.
The trace reports page boundaries, glyphs, rules, and opaque specials. Exit
codes are 0 success, 1 document/metrics/execution failure, 2 usage error, and
3 file/stream I/O failure. A sink error may leave a printed prefix.

## Optional tools and source generators

```text
vmweave-metaml INPUT.vmweave OUTPUT.mvmb
vmweave-metaml --definition INPUT.vmweave GUEST.bin OUTPUT.mvmb
vmdesc FILE.vmdesc
python3 tools/generate_catalog.py [--check] [--output-root DIRECTORY]
python3 tools/generate_examples.py --vmweave EXECUTABLE --output-root DIRECTORY
python3 tools/generate_examples.py --list-inputs
python3 tools/generate_vmspec_manual.py --check
```

Optional executables exist only when enabled. MetaML compilation returns 1 on
evaluation/lowering/output failure and 2 on invalid usage. VMDesc prints the
compiled document's title/text; it is not an automatic PDF renderer. Generator
`--check` is read-only and returns nonzero for drift; ordinary generation edits
only its owned outputs. Chapter 24 explains their different responsibilities.
