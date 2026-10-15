# Chapter 21 — The mvect Driver

**Status:** check pipeline implemented (`driver/mvect.d`,
`driver/mvect-cli.d`); full compilation stubbed.

## `compile(options)`

`CompileOptions` carries `inputPath`, `backend` (default `"auto"`),
`optimize`, `debugIR`, and `checkSyntaxOnly`. `compile` reads the file
itself (the only filesystem touch in the frontend), parses, runs the
semantic checks, and then:

- with `checkSyntaxOnly`, returns success and no output on a clean tree;
- otherwise returns failure explaining that lowering, MVIR, and emission
  are not implemented — unless the source itself failed first, in which
  case the source diagnostics are returned instead.

`CompileResult` is `{ success, outputPath, diagnostics[] }`. No output
file is ever produced today.

## The CLI

`mvect --check-syntax [--] <input.mvct>` checks and stays silent on
success. `mvect --version` prints `mvect 0.1.0`; `mvect --help` prints
usage. Unknown flags exit `2`; two input files exit `2`; `--` ends flag
parsing so oddly-named files still check. Argument parsing lives in
`mvect-cli.d`, compilation in `mvect.d` — the separation is deliberate and
tested, including the missing-file and unreadable-encoding paths.
