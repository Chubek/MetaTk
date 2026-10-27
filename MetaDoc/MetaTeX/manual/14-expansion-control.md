# 14 — Controlling expansion

`\expandafter` saves the next token, expands the following token once if it is
expandable, and then restores the saved token in front of the result. It does
not mean “fully expand the rest of the line.”

<!-- metatex-example {"format":"plain","text":"[A]"} -->
```tex
\def\value{A}
\def\wrap#1{[#1]}
\expandafter\wrap\value
```

The single expansion of `\value` supplies `A` before `\wrap` collects its
argument. Chained `\expandafter` operations can express more complicated
ordering, but they should be accompanied by small tests: an extra token can
change which command expands first.

`\noexpand` protects the next expandable token for one expansion step. It is
especially useful inside `\edef`; outside an expanded definition, a protected
expandable command behaves inertly for that step. Applying it to an
unexpandable command does not prevent that command's ordinary execution.

The engine distinguishes expansion from execution. Arbitrary plugin handlers
are unexpandable and are not invoked by the expansion scanner. A host may
replace a built-in command through `registerCommand`; that replacement also
uses handler semantics. Nested expansion is bounded independently of the
overall token-processing budget.

[Contents](README.md)
