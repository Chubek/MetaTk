# 12 — Delimited parameters

A parameter followed by literal tokens reads input until that delimiter is
matched outside nested groups. Delimiters may be several characters or control
sequences, and macro definitions can also require a literal prefix.

<!-- metatex-example {"format":"plain","text":"second/first"} -->
```tex
\def\swap#1,#2;{#2/#1}
\swap first,second;
```

Here the comma terminates `#1`, and the semicolon terminates `#2`. A comma or
semicolon inside a balanced group is protected. If the entire collected
argument is one group, its outer braces are removed; several adjacent groups
are retained as separate groups.

The special `#{` ending of a parameter list makes the opening group token a
delimiter and appends that token to the replacement. This supports definitions
that hand an opening group back to later processing. Ordinary custom commands
rarely need that form; use a visible punctuation delimiter where possible.

Matching compares tokens, not merely printed spelling. A category-11 `@` does
not match a category-12 `@`; active and escaped control symbols differ too.
Overlapping multi-token delimiters are matched in linear time. Missing
delimiters and mismatched literal prefixes are errors, not instructions to
silently consume the rest of a document.

[Contents](README.md)
