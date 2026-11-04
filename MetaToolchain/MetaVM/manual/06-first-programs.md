# Chapter 6 — Your first programs

[Previous](05-cmake-builds.md) · [Contents](README.md) · [Next](07-command-line-reference.md)

## Run the supplied factorial

After chapter 5's build:

```sh
out-manual/Machinery/metavm-run Machinery/examples/factorial.mvm
out-manual/Machinery/metavm-run Machinery/examples/factorial.mvm --threaded
```

Both print `3628800`. The default CMake layout places the runner in the
`Machinery` subdirectory. In the fallback build it is `build/metavm-run`.
Changing dispatch engines must not change integer arithmetic or program results.

## A complete arithmetic module

Save this as `answer.mvm`, or use [the supplied example](examples/answer.mvm):

```text
.function main 0 3
const r0 6
const r1 7
mul r2 r0 r1
ret r2
.end
```

The function takes zero arguments and owns three registers. `const` creates
integer values; `mul` writes their product; `ret` returns the selected register.
All nonargument registers start as nil, so removing one constant would cause
a runtime type failure, not implicitly multiply by zero.

```sh
out-manual/Machinery/metavm-run answer.mvm
out-manual/vmweave compile answer.mvm answer.mvmb
out-manual/Machinery/metavm-run answer.mvmb
out-manual/vmweave emit-c answer.mvmb answer_factory.c application_answer
```

The first and third commands print `42`. The compiler auto-detects Machinery
binary input when loading a file. Emitted C defines a module factory, not a
standalone `main`; it must be compiled and linked with Machinery, then called
by a host. Chapter 24 shows the ownership and link model.

## A first native subset

```sh
out-manual/ssavm-run VMLibrary/SSAVM/fibonacci.ssa
out-manual/ssavm-run --check VMLibrary/SSAVM/fibonacci.ssa
out-manual/closure-vm-demo
out-manual/vmweave-definition-demo
```

Expected outputs are respectively `55`, `verified`, `secd=42 krivine-term=1`,
and `result=42 memory[3]=42`. These demonstrate different interfaces:
SSAVM textual execution, opt-in static analysis, native in-memory closure
machines, and Definition lowering. They are not interchangeable input formats.

## Diagnose the first failure instead of guessing

An assembly syntax error has a one-based source line. A verification/runtime
error may identify a zero-based function and instruction. A missing import
means the host has not supplied an exact name/arity match; the generic runner
does not automatically bind graphics or application callbacks.

If the command is missing, check its build option and output directory. If a
program never reaches a return, preserve the instruction limit and inspect
its condition: Machinery treats integer zero as true. Compare an integer
counter explicitly to zero before using `jz`.

**References:** [assembly](10-assembly-language.md), [runnable SDK examples](examples/README.md).
