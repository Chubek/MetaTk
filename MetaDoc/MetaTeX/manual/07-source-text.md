# 07 — Source text and input notation

Source must be valid UTF-8. Malformed encodings and literal NUL bytes are
rejected. Ordinary spaces collapse, control words consume following source
spaces, and a blank line produces a paragraph. A comment character discards
the rest of its physical line, including that line's ending.

<!-- metatex-example {"format":"plain","text":"AB"} -->
```tex
A% this line ending does not insert a space
  B
```

Doubled superscript notation is translated before category lookup, including
inside a control word. With the default category of `^`, `^^41` is `A` and
`^^M` is a carriage-return character. Two lowercase hexadecimal digits select
the hex form; otherwise the following ASCII character is shifted by 64.

<!-- metatex-example {"format":"plain","text":"ABC"} -->
```tex
^^41^^42^^43
```

The notation follows the current superscript category: after changing `^` to
category 12, it is literal text. A translated comment or end-line character
has its normal scanner effect. Comment contents themselves are not translated.
Source offsets and diagnostic columns still refer to the original spelling.

Native input normalization remains narrower than TeX82: `\endlinechar`, its
synthetic final-line behavior, and all TeX line-buffer conventions are not
implemented. Use explicit braces, spaces, or `\relax` to terminate numeric
input rather than depending on an implicit end-of-file character.

[Contents](README.md)
