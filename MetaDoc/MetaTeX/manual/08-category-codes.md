# 08 — Category codes

`\catcode CHARACTER = CATEGORY` changes how unread source is tokenized.
CHARACTER is an integer from 0 through 255; CATEGORY is from 0 through 15.
Backtick syntax supplies a character's numeric code conveniently:

<!-- metatex-example {"format":"plain","text":"OK"} -->
```tex
\catcode`\@=11
\def\internal@name{OK}
\internal@name
```

| Code | Role | Code | Role |
|---|---|---|---|
| 0 | Escape | 8 | Subscript |
| 1 | Begin group | 9 | Ignored |
| 2 | End group | 10 | Space |
| 3 | Math shift | 11 | Letter |
| 4 | Alignment | 12 | Other character |
| 5 | End line | 13 | Active character |
| 6 | Parameter | 14 | Comment |
| 7 | Superscript | 15 | Invalid character |

Assignments are local inside a group. `\global\catcode` survives group exit,
including overriding a previously saved local value. Query a category with
`\the\catcode` or use it where an integer is expected.

Tokens already read are unchanged. In particular, the integer scanner may
read one terminating token before an assignment takes effect. Put a space
after the assigned number when the following character must use its new
category. Native Unicode characters above 255 remain letter-like; this
assignment interface does not configure their categories.

[Contents](README.md)
