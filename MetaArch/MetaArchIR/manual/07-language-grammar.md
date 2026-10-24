# Chapter 7 — Language grammar and statement structure

## Line-oriented source

A `.mair` file contains at most one statement per line. LF and CRLF line endings
are supported, and the final line need not end with a newline. A label takes its
own line: `entry: encode ret` is not a combined label/instruction production.

The following EBNF summarizes the intended statement syntax. Whitespace between
tokens is omitted for readability; quoted operands and native addressing syntax
are described in later chapters.

```ebnf
statement = identifier, ":"
          | directive, arguments
          | identifier, [native_operand, {",", native_operand}] ;
directive = ["."], ("section" | "global" | "globl" | "align" | "balign"
          | "ascii" | "byte" | "word" | "long" | "quad" | "encode" | "form")
          | ".2byte" | ".4byte" | ".8byte" ;
binding   = identifier, "=", (integer | identifier | symbol_reference) ;
encode    = ["."], "encode", identifier, {",", binding} ;
form      = ["."], "form", nonnegative_integer, {",", binding} ;
```

The `encode` and `form` productions expand their corresponding directive cases;
each other directive has its own argument contract. This is a grammar summary,
not a permissive rule that lets every directive take arbitrary arguments.

## Dispatch and separators

Directive names are recognized before native instruction dispatch. For example,
`byte` means a data directive, not an instruction named `byte`. A leading dot is
optional for named directives. Numeric aliases require the dot because an
identifier cannot begin with a digit.

Use commas between operands and bindings. Empty operands and trailing commas
are errors. Commas inside balanced native operand delimiters or double quotes
are protected from splitting.

There are no semicolon-separated statements, block syntax, includes, macros or
general arithmetic expressions in checked bindings. Applications that generate
assembly can calculate their immediate values before building or parsing a
program. Source comments are discarded rather than preserved as formatting data.

[Previous](06-target-inspection.md) · [Contents](README.md) · [Next](08-names-and-comments.md)
