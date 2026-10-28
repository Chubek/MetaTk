# 16. Effects, traps, and assumptions

Two computations can have the same result and still be observably different.
Juniper preserves evaluation effects and possible traps through its selection and
scheduling stages, and relies on MetaIL's guarded rules for optimization.

## Calls have effects unless proved otherwise by a different system

<!-- check: juniper -->
```juniper
@fn(ordered, (x))
  @let(first, begin_sample())
  @let(value, add(x, first))
  @eval(end_sample(value))
  @return(value)
@end
```

The compiler does not know that `begin_sample` and `end_sample` are harmless.
Their `CALL` instructions are full barriers. Arithmetic depending on their
results keeps its dependencies; independent arithmetic cannot cross the full
effect fence merely because doing so would reduce a modeled cycle count.

## Traps are observable too

<!-- check: juniper -->
```juniper
@fn(division_before_notification, (x, divisor))
  @let(value, div(x, divisor))
  @eval(notify())
  @return(value)
@end
```

If division cannot complete normally, moving `notify()` ahead of it changes what
is observed. The lowerer conservatively marks division, remainder, and shifts as
barriers, including cases where a more elaborate proof might show them safe.
The current scheduler does not independently prove those expressions total.

Never replace `mul(call(), 0)` with `0` or `div(x, x)` with `1` without the required
effect/trap reasoning. For example:

<!-- check: juniper -->
```juniper
@fn(known_number_with_effect, ())
  @return(band(tick(), 0))
@end
```

Its successful value is known, but evaluating the call is still part of the
program. A numeric fact about a result is not a license to discard evaluation.

## What assume currently means for this pipeline

<!-- check: juniper -->
```juniper
@fn(annotated_division, (x, divisor))
  @assume(ne(divisor, 0))
  @return(div(x, divisor))
@end
```

`assume` is parsed and preserved, and selection emits a full-fence `ASSUME`
pseudo-instruction. It is not currently used to refine a general path-sensitive
fact lattice. Do not expect this annotation to remove the division fence, prove
absence of signed overflow, or synthesize a runtime assertion implementation.
An eventual executor/backend must give its assumption handling an explicit
contract; the Juniper CLI does not execute it.

## Review transformations with an effect checklist

For every proposed rewrite or fused instruction, ask:

1. Is every original argument evaluated the same number of times?
2. Are effectful arguments evaluated in the same order?
3. Are potentially trapping operations kept on the same side of effects?
4. Does each lazy arm remain conditional?
5. Does wrapping arithmetic still produce the same 64-bit result?

MetaIL's purity guards incorporate traps as well as calls. Use its tested
kernels rather than introducing an unguarded shortcut in a macro or C++ visitor.

[Previous](15-lazy-select.md) · [Index](README.md) · [Next](17-canonical-ir.md)
