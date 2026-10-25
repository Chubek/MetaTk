# Chapter 13 — Reserved Forms and Rejected Syntax

**Status:** reference for what the language deliberately refuses today.

## Reserved keywords without grammar

`type`, `forall`, `match`, `with` lex as keywords but start no construct.
Each is reserved for a later milestone (user types, polymorphism,
pattern matching). Using them as expressions fails:

```mvct
x = match y with z   -- rejected: match-expressions are not implemented
x : forall a. a = f  -- rejected: no quantifier syntax yet
type X = Int         -- rejected: no type declarations yet
```

## Tokens without expression meaning

`[ ] { } :: | &` and `->` (outside type annotations) lex successfully but
are rejected by the parser: `x = [1]`, `x = {1}`, and `x = 1 }` all fail.
This is the pipeline boundary doing its job — the lexer accepts the
character, the grammar declines the shape.

## Common rejections

| Input | Why it fails |
| --- | --- |
| `x = 1.` | `1.` is integer `1` plus `.`; a trailing dot starts no expression |
| `x = f(1,)` / `f(,1)` | No trailing or leading commas in argument lists |
| `x = fn(x,) => x` | No trailing commas in parameter lists either |
| `x = fn x => x` | Parameter lists require parentheses |
| `x = 1 2` | One binding, one value; juxtaposition is not application |
| `x = 1; ;` | Stray semicolons start no binding |
| `x = 1 @` | `@` is not a MetaVect character at all |

Tuple, record, and collection literals do not exist yet; `()` remains the
only composite literal.
