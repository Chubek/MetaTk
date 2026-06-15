# 03 Grammar Basics

A Parzek grammar is a text file (`.pzg`) with directives and rules.

## Mandatory directive

Every grammar must set parser name:

```pzg
@parser:name(MyParser)
```

## Rule forms

```pzg
TOKEN_NAME: expression;
rule-name: expression;
```

Naming policy:

- lexical rule: `ALL_CAPS`
- syntactic rule: `kebab-case`

Parzek validates this distinction.

## Primary atoms

- string literal: `"foo"`
- character class: `[0-9]`
- rule reference: `TOKEN_NAME` or `rule-name`

## Composition operators

- sequence: adjacency in expression
- ordered choice: `|`
- zero-or-more: `*`
- one-or-more: `+`
- optional: `?`
- grouping: `( ... )`

## Example

```pzg
@parser:name(Calc)
WS: [ \t\r\n]+ -> @IGNORE;
NUMBER: [0-9]+;
expr: NUMBER (("+" | "-") ~ NUMBER)*;
```

## Parse entrypoint (start symbol)

Parzek resolves parser start symbol with this precedence:

1. `@parser:start(rule-name)` if present
2. otherwise, first user-defined non-terminal (first syntactic rule)
3. fallback to first rule if no non-terminal exists

Example with explicit override:

```pzg
@parser:name(MyGrammar)
@parser:start(main)
TOKEN: "x";
main: TOKEN;
```

If both the top non-terminal and `@parser:start(...)` exist and differ, the directive wins.
