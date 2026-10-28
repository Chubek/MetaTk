# 7. Defining and expanding macros

Use macros to generate regular source patterns, not to replace semantic analysis.
The syntax profile is already configured by Juniper; do not invoke GPPX with its
unrelated default/C/TeX syntax and expect the same source to work.

## Object-like and parameterized macros

<!-- check: juniper -->
```juniper
#define @word_mask 255
#define @double mul(#1, 2)
#define @bias add(#1, #2)
@module(macros)
@fn(prepare, (x))
  @let(doubled, @double(x))
  @return(band(@bias(doubled, 7), @word_mask))
@end
```

Numbered references `#1` through `#9` refer to macro arguments. In this profile,
write the `@` in a `#define` macro name, as shown. You do not need a C-style
formal-parameter declaration for these numbered macros.

Inspect expansion before investigating an optimizer problem:

```sh
"$J" preprocess "$WORK/macros.jun" -o "$WORK/macros.expanded"
"$J" check "$WORK/macros.jun"
```

Inclusion markers and blank lines in the expanded file are expected. Canonical
`emit` is the operation that normalizes the complete IR.

## Expression macros versus statement macros

Expression macros should usually expand to one explicitly grouped expression:

<!-- check: juniper -->
```juniper
#define @plus_one (#1 + 1)
#define @assign_plus_one let #1 = @plus_one(#2);
@fn(step, (input))
  @assign_plus_one(result, input)
  @return(mul(result, 3))
@end
```

The `@plus_one` grouping prevents an enclosing multiplication from changing its
meaning. `@assign_plus_one` includes a semicolon because it emits a statement.
Keep the distinction visible in macro names and examples.

## Prefer a small vocabulary

The standard library already gives you `@module`, `@fn`, `@end`, `@let`,
`@return`, `@eval`, `@assume`, `@label`, `@goto`, `@br`, and backend declarations.
Add a macro only when it makes a repeated structure clearer. For example:

<!-- check: juniper -->
```juniper
#define @affine add(mul(#1, #2), #3)
@fn(pixel_offset, (row, stride, column))
  @return(@affine(row, stride, column))
@end
```

Prefix notation already groups arguments, so it is often simpler and safer in
generated expression macros than concatenating infix tokens.

## What macros do not guarantee

Macros do not check operand types, protect local names, prove purity, or guarantee
single evaluation. The following definition is textually valid but can change
effect counts:

```text
#define @square mul(#1, #1)
```

`@square(tick())` generates two calls. If the intended operation calls `tick`
once, bind its result with `@let` first. The next chapter develops this rule into
a practical discipline for reusable macros.

[Previous](06-source-syntax.md) · [Index](README.md) · [Next](08-macro-parameters.md)
