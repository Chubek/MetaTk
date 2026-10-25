# Chapter 9 — Literals and Expressions

**Status:** syntax implemented; no evaluation.

## Literals

| Form | Example | Note |
| --- | --- | --- |
| Integer | `42` | Exact spelling kept; never converted to a host value |
| Float | `1.25`, `1e3` | See chapter 5 for shapes |
| String | `"hi\n"` | Escapes kept raw; never decoded |
| Boolean | `true`, `false` | Lexed as keywords, read as literals |
| Unit | `()` | Two paren tokens; parser reads the pair as unit |

Out-of-range values such as `999…9` (40 digits) or `1e99999` are accepted
as spellings; range is a semantic question for later.

## Expression forms

```mvct
identity(base + 2 * 3)          -- call, possibly chained: f()(1, g(2))
(1 + 2)                         -- grouping
-f(1)                           -- prefix: +, -, !
if base > 0 then 1 else 0       -- conditional (both branches required)
let x : Int = 1 in x + y        -- local binding
fn(x: Int, y) => x + y          -- lambda (fn or fun)
```

Calls apply to any expression, so `f()(1)` and `ffi("must not execute")`
parse — and the second never executes anything, because parsing never
executes. A name followed by juxtaposition (`f x`) is *not* application;
calls require parentheses.

## What expressions never do

Fold, convert, or canonicalize. `-0.0` stays the spelling `-0.0` applied
to `0.0`; `"a" + "b"` is a binary tree, not a string operation. Meaning
arrives with later milestones.
