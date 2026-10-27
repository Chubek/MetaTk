# 17 — Registers and measurements

## Integer registers

There are 256 native integer registers, addressed as `\count0` through
`\count255`. Unassigned registers read as zero. Assignment permits an optional
equals sign; `\the` expands a register's current value into decimal tokens.

<!-- metatex-example {"format":"plain","text":"7/9"} -->
```tex
\count0=7
\edef\saved{\the\count0}
\count0=9
\saved/\the\count0
```

Use `\countdef` to give a register a command name:

<!-- metatex-example {"format":"plain","text":"42"} -->
```tex
\countdef\answer=42
\answer=42
\the\answer
```

The alias refers to a register index, not a copy of its current value. Aliases
and assignments are group-local unless prefixed with `\global`. A global
assignment removes earlier local saves for that register; a later local
assignment can still temporarily override it.

Registers persist across `compileFragment` submissions but reset for a fresh
`compile` call. They are separate from LaTeX-style named counters accessed
with `\setcounter` and `\arabic`. The dimension, glue, and token-list banks
below are distinct from the integer bank and from each other. Each has 256
slots, its own alias declaration, local/global assignment, and `\the` access.
`\let` between register aliases copies the register identity, so later
redefining the original alias does not redirect the copy.

## Dimensions

`\dimen0`–`\dimen255` store fixed-point distances. `\dimendef` names a slot.
One point is 65536 scaled points (`sp`); the largest magnitude is 1073741823sp,
slightly less than 16384pt. Decimal fractions use TeX's rounding rules rather
than host floating-point arithmetic. A comma can replace the decimal point.

<!-- metatex-example {"format":"plain","text":"1.5pt/98304"} -->
```tex
\dimendef\length=0
\length=1.5pt
\the\length/\number\length
```

`\the` prints a round-trippable point value; `\number` prints the underlying
scaled-point integer. Physical units are `pt`, `sp`, `in`, `pc`, `cm`, `mm`,
`bp`, `dd`, and `cc`, case-insensitively. Font-relative `em`/`ex` and `true`
magnification units are not yet implemented and produce diagnostics.
Use `\dimen1=.5\dimen0` to scale an internal distance. An integer register
before a unit supplies the coefficient; an integer used as the unit supplies
a scale in sp. Fractional `sp` input truncates its fraction, as in TeX.

## Glue

`\skip0`–`\skip255` store natural width, stretch, and shrink, with aliases
declared by `\skipdef`. Literal syntax is a dimension optionally followed by
`plus` and `minus` components. Those components accept finite distances or
the infinite orders `fil`, `fill`, and `filll`.

<!-- metatex-example {"format":"plain","text":"2.0pt plus 1.0fil minus 0.5pt"} -->
```tex
\skipdef\gap=0
\gap=2pt plus 1fil minus .5pt\relax
\the\gap
```

Use `\relax` to terminate a glue expression when an expandable command follows:
the scanner looks ahead for optional `plus` and `minus` keywords and can expand
that command before storing the assignment. Direct register assignment copies
all three components. A minus sign on a glue register negates all components;
in a literal `-2pt plus 1fil`, only the width is negative. In a numeric or
dimension context, a glue register supplies its natural width.

## Token lists

`\toks0`–`\toks255` store unexpanded, category-preserving token lists.
`\toksdef` declares an alias. Assign a braced list or another token register.
Copying retains the list's contents independently of later source-register
assignments; immutable backing storage is shared to avoid copy amplification.

<!-- metatex-example {"format":"plain","text":"new"} -->
```tex
\def\word{old}
\toks0={\word}
\edef\saved{\the\toks0}
\def\word{new}
\saved
```

Ordinary `\the` inserts the list into the expansion stream. When directly
scanning an `\edef`/`\xdef` replacement, `\the` copies the list without further
expansion or reinterpretation of its parameter characters. Token registers
do not support arithmetic or conversion to numbers.

Box registers, math-glue (`\muskip`) registers, and integration of these
distances/glues into a full TeX layout tree remain outstanding. Storing a glue
value does not by itself alter paragraph spacing or PDF geometry.

[Contents](README.md)
