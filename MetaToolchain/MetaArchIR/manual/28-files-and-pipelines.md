# Chapter 28 — Files, pipelines and exit statuses

## Standard streams

```sh
printf 'encode ret\n' | build/marchir -T amd64 --db MetaArchDB --emit hex -
build/marchir -T aarch64 --db MetaArchDB --emit asm \
  MetaArchIR/examples/cli/aarch64/add.mair -o -
```

The first command prints `c3` and a newline. The second writes assembler text to
stdout even though the input is a file. Binary output contains only bytes, with
no textual newline or status message appended. Hex output is lowercase hex with
a final newline.

Use `--` before a filename beginning with a dash. The CLI accepts one input,
not a collection of compilation units. Combine files explicitly in a higher-level
workflow if that is what your application needs, keeping source-location and
duplicate-label consequences in mind.

## Exit status contract

| Status | Meaning |
| --- | --- |
| 0 | Success |
| 1 | Invalid source or target, including a missing target specification |
| 2 | Invalid usage, unknown options or incompatible options |
| 3 | Input/output or filesystem operation failure |

Not all missing files follow the same route: an unavailable compilation input
is an I/O error, while inability to load the requested target is reported as a
target error. Do not classify failures solely by searching diagnostic text.

## Output preservation

The CLI parses and generates its result before opening the output file. Source
or encoding errors therefore leave an existing output file untouched. It also
rejects output paths that name the input, including equivalent existing files.
Once writing begins, a failed write can still leave partial output: this is not
an atomic temporary-file-and-rename guarantee.

The C++ `compile_file(input, output, target, limits)` helper follows the text
rendering path and checks input/output aliasing. It has no format selector;
applications that need bytes or images should call `parse` and `assemble`, then
perform their chosen output serialization.

[Previous](27-cli-reference.md) · [Contents](README.md) · [Next](29-resource-limits.md)
