# Chapter 25 — Backend Selection

**Status:** scaffold contracts (`compiler/CSelReader.d`,
`compiler/CSelRuntime.d`; rules in `stdbe/*.csel`).

## Selector rules

Each backend ships a `.csel` file mapping MVIR shapes to that backend's
constructs. `CSelReader` will read and validate these rules, rejecting
unknown directives with file-and-line context rather than guessing.

## Predicate evaluation

`CSelRuntime` will evaluate selection predicates against explicit target
capabilities — never arbitrary host code. A predicate sees the term shape
and a capability table (supported primitives, numeric behavior, text
handling) and answers which backend rule applies. Anything undecidable is
a diagnostic, not a default.

## The `.csel` catalog

Eleven selector files exist, one per backend in chapter 26 (Asymptote,
Cairo, Clipper2, LaTeXPicture, NanoVG, PSTricks, PostScript, Skia, TikZ,
TROFF, plus the Cairo entry). They are placeholders awaiting the term
model they will select on.
