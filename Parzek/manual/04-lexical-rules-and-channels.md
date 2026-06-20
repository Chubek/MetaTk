# 04 Lexical Rules and Channels

Channels are used to control skipping behavior and predicate context.

## Built-in channel

- `@IGNORE` is built-in.

Typical whitespace rule:

```pzg
WS: [ \t\r\n]+ -> @IGNORE;
```

## Additional channels

You can direct tokens to custom channels:

```pzg
COMMENT: "#" [a-zA-Z0-9_ ]* -> @COMMENT;
KEYWORD: "if" -> @TOK;
```

Channels are implicitly introduced on first use.

## Channel-sensitive behavior

- Ordinary sequencing can allow skip behavior based on active parsing mode.
- `~` adjacency explicitly forbids ignored-material gap between neighbors.
- Predicate context exposes `CHAN`.

Example:

```pzg
pair: A ~ B when CHAN == @TOK;
```

## Practical recommendation

- Route whitespace/comments to `@IGNORE`.
- Keep lexical channel use simple unless you need predicates tied to channel state.
- Reserve `~` for real adjacency requirements (keywords, punctuators, tight token joins).
