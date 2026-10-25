# Chapter 3 — Your First Program

**Status:** implemented up to syntax checking. Nothing here is evaluated or
rendered yet.

## The example

`examples/first.mvct` is the canonical first program:

```mvct
-- First milestone: parse only; none of these expressions are evaluated.
module examples.first
import prelude

export identity : Int -> Int = fn(value: Int) => value
let scale = fun(value: Float, factor: Float) => value * factor
export size : Int = let base : Int = 20 in
    if base > 0 then identity(base + 2 * 3) else 0
export label : String = "MetaVect\nsyntax frontend"
```

It declares a module, imports `prelude`, and defines four bindings: two
functions, a conditional size, and a string label.

## Checking it

```sh
build/mvect --check-syntax examples/first.mvct
build/mvect --version
```

A successful syntax check is silent and exits `0`. Diagnostics go to
stderr in `file:line:column: error: message` form.

## Exit codes

| Code | Meaning |
| --- | --- |
| `0` | Success, `--help`, or `--version`. |
| `1` | Syntax, semantic, or input-read failure in `--check-syntax` mode. |
| `2` | Invalid command-line usage (missing file, unknown option, two inputs). |
| `3` | Compilation failed without `--check-syntax` — either a source error or
  the not-yet-implemented remainder of the pipeline. |

A plain `build/mvect examples/first.mvct` (no `--check-syntax`) exits `3`:
the frontend runs, then the driver honestly reports that lowering, MVIR,
and emission are not implemented. It produces no output and does not
pretend to render graphics.

## Try breaking it

```sh
printf 'export broken =\n' > /tmp/broken.mvct
build/mvect --check-syntax /tmp/broken.mvct; echo "exit: $?"
```

Expected: `/tmp/broken.mvct:2:1: error: expected expression, found end of
file` and exit `1`. Chapter 20 explains every diagnostic kind.
