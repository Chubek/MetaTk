# 06 Predicates, Meta Variables, and Helpers

Parzek supports `when` guards and meta-variable driven checks.

## Guard syntax

```pzg
INTEGER: [0-9]+ when $0 != '0';
```

Guard attaches to the nearest expression node.

## Supported meta variables

Current supported family includes:

- `$0` (first character-oriented checks in current rule text)
- `LEN` (length-oriented checks)
- `FILE`
- `LINE`
- `COLUMN`
- `CHAN`

For complex grammars, document your guard assumptions near rule definitions.

## Predicate-side helper intent

Parzek keeps helpers constrained to parsing/predicate support, including:

- `when` guard wrapper
- helper combinators in generated support block (`many1`, `between`, `sep_by`, `sep_by1`)

The project avoids turning predicates into a full scripting runtime.

## Practical examples

```pzg
NONZERO: [0-9]+ when $0 != '0';
TOKEN: "foo" when CHAN == @TOK;
```

## Debugging guard failures

When guards fail, parsing rewinds according to combinator failure semantics and diagnostics should indicate expected guard path context from labeled parsers.
