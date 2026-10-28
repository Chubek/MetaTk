# 6. Source syntax and the two language layers

Juniper source combines a GPPX macro layer with a versioned IR layer. Be explicit
about which layer a piece of syntax belongs to.

## The outer layer

* `@name(...)` expands a user macro; `@name` can expand an object-like macro.
* `#define`, `#include`, and conditionals are GPP directives.
* Parentheses protect commas while arguments are read.
* Preprocessing removes `//` and `/* ... */` comments and protects quoted strings.

Ordinary words do not expand just because they match a macro's name:

<!-- check: juniper -->
```juniper
#define @factor 8
@module(syntax)
@fn(scale, (factor))
  // The local factor stays a local; @factor expands to the literal 8.
  @return(mul(factor, @factor))
@end
```

This deliberate `@` prefix helps separate compile-time text generation from IR
variable naming. Macro expansion is still textual, not a typed AST operation.

## The inner layer

After preprocessing, input has this layout:

```text
[juniper 1;]
[backend declarations]
[module name;]
fn function_name(parameters) {
  statements
}
...more functions...
```

Square brackets above mean optional sections, not literal tokens. Version and
backend declarations must come before the MetaIL module/function body. There may
be only one backend declaration. At least one function is required.

<!-- check: juniper no-preprocess -->
```juniper
juniper 1;
backend syntax_profile extends portable {
  issue 2;
  resource alu 2;
}
module syntax;
fn scale(factor) {
  return factor * 8;
}
```

Mixing prefix and infix operator spellings is allowed in function bodies. Backend
patterns, however, use canonical names such as `add` and `mul`, not infix syntax.

## Names, punctuation, and literals

IR identifiers use `[A-Za-z_][A-Za-z0-9_]*`. Case matters. Generated machine names
contain `$`, but that does not make `$temporary` a legal source variable.
Functions and backend blocks use braces; most IR statements and metadata entries
end in semicolons; labels end in colons.

Metadata integers are signed-64 **decimal** values. The MetaIL expression parser
also accepts hexadecimal literals, including 64-bit bit patterns. Keep metadata
and program literal syntax separate:

<!-- check: juniper no-preprocess -->
```juniper
backend spelling extends portable {
  rule byte_add : add(reg, imm(0, 255)) => ADD_BYTE cost 1;
}
fn low_byte(x) {
  return band(add(x, 7), 0xff);
}
```

String literals are not general scalar values in the program language. Include
filenames are quoted strings at the macro layer; writing `return "hello";` does
not create a valid MetaIL string value.

With `--no-preprocess`, prefer `//` comments. The MetaIL body accepts additional
line-comment forms, but C-style block comments are normally removed by GPPX;
do not assume the raw-IR parser handles every preprocessor comment convention.

[Previous](05-command-line.md) · [Index](README.md) · [Next](07-macro-basics.md)
