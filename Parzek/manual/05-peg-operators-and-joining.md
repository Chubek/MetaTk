# 05 PEG Operators and Joining

This chapter summarizes Parzek operator semantics and generated combinator mapping.

## Ordered choice (`|`)

PEG choice is ordered:

```pzg
value: NUMBER | IDENT;
```

The left branch is preferred; right branch runs on non-committed failure.

## Sequence

Adjacent terms create sequence:

```pzg
assign: IDENT "=" value;
```

Generated code composes parser sequence and combines parse payload.

## Repetition

- `*` => zero-or-more
- `+` => one-or-more (via `many1` helper)
- `?` => optional

Example:

```pzg
digits: [0-9]+;
list: item ("," item)*;
```

## Grouping

Parentheses control precedence:

```pzg
factor: NUMBER | "(" expr ")";
```

## Adjacency (`~`)

`~` requires direct adjacency under ignore/channel interpretation:

```pzg
joined: "a" ~ "b";
```

Use adjacency for tight lexical/syntactic constraints where regular sequencing is too permissive.

## Readability advice

- Prefer explicit groups around mixed `|` and sequence.
- Limit deeply nested expression lines.
- Split large grammars into thematic rule blocks.
