# 4. Your first Juniperfile

Start with one function and no custom backend. This keeps macro mistakes separate
from instruction-selection mistakes.

## Create the program

Save the following as `$WORK/Juniperfile`:

<!-- check: juniper -->
```juniper
@module(first_program)
@fn(affine, (x, multiplier, offset))
  @let(product, mul(x, multiplier))
  @return(add(product, offset))
@end
```

`@fn` takes two macro arguments. The second is a **parenthesized parameter list**;
the grouping keeps its internal commas inside one macro argument. `@end` closes
the function body. Macro statements already expand to semicolons, so you need
not add a semicolon after `@let` or `@return`.

Validate, then inspect:

```sh
"$J" check "$WORK/Juniperfile"
"$J" metail "$WORK/Juniperfile" -o "$WORK/first.mi"
"$J" emit "$WORK/Juniperfile" -o "$WORK/first.junir"
```

Successful `check` is silent. The emitted MetaIL body is:

<!-- check: juniper no-preprocess -->
```juniper
module first_program;
fn affine(x, multiplier, offset) {
  let product = (x * multiplier);
  return (product + offset);
}
```

The complete `.junir` additionally contains version and backend declarations.
It is longer because the portable selection rules are made explicit.

## Select and schedule

```sh
"$J" select "$WORK/Juniperfile" -o "$WORK/first.selected.json"
"$J" schedule "$WORK/Juniperfile" -o "$WORK/first.scheduled.json"
python3 -m json.tool "$WORK/first.scheduled.json"
```

Expect `MUL`, a `COPY` assigning `product`, `ADD`, and `RET`. Generated temporaries
look like `$v0`; they are not physical registers. Selected instructions have
`cycle: null`; scheduled instructions have integer cycles and dependencies.
Do not interpret the JSON file as a native executable or as input to `emit`.

## Use the default filename

Inside the working directory, the filename is optional:

```sh
cd "$WORK"
"$J" check
"$J" emit -o default-name.junir
```

`Juniperfile` has no special build-recipe syntax. It is simply the default input
name. Files named `affine.jun`, `affine.junir`, or another extension are processed
according to the selected command and flags, not an automatic extension mode.

**Try it:** replace the return with `add(product, 0)`, optimize it using
`--passes AddZeroRight`, and compare the MetaIL output before and after. The
operation is compiled, not executed: numerical evaluation is not a CLI feature.

[Previous](03-installation-and-runtime.md) · [Index](README.md) · [Next](05-command-line.md)
