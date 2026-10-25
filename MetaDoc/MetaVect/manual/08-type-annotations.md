# Chapter 8 — Type Annotations

**Status:** type *syntax* implemented; type *checking* is future work
(System F is the design target).

## Syntax

```ebnf
type      = type_atom, ["->", type] ;
type_atom = identifier | "(", type, ")" ;
```

`->` associates to the right: `Int -> Float -> Bool` reads as
`Int -> (Float -> Bool)`. Parentheses override grouping:
`(Int -> Float) -> Bool`. Annotations appear on bindings, lambda
parameters, and `let` names:

```mvct
export identity : Int -> Int = fn(value: Int) => value
```

## Names mean nothing yet

Type names are ordinary identifiers to the lexer and parser. `Int`,
`String`, `Path`, `Color`, `Vector`, and `Matrix` are conventional —
highlighted as types by the editors — but no table of known types exists
and no mismatch is reported.

## Reserved for later

`type` and `forall` lex as keywords but have no grammar yet: `type X =
Int` and `forall a. a` are rejected. They are reserved for user-declared
types and System-F-style polymorphism. Likewise `match`/`with` are
reserved for a future pattern-matching form. Any program using them today
fails with a syntax error; that is intentional reservation, not a bug.
