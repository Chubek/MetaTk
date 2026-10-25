# Chapter 27 — Emission

**Status:** scaffold contract (`compiler/Emitter.d`).

## Interface shape

The emitter takes three things: a rewritten MVIR module, a selected
backend (chapter 25), and explicit output options (format, destination,
numeric policy). It returns bytes plus diagnostics, or diagnostics alone.
There is no ambient configuration — no environment variables, no current
directory sniffing, no implicit backend.

## Diagnostics contract

Emission failures are backend facts with source spans where the offending
construct originated: unsupported primitives, unrepresentable text runs,
numeric behavior the target cannot honor. Partial output is never silently
kept; the driver reports failure and leaves no half-written file behind
(that guarantee constrains the future `compile()` implementation in
chapter 21).

## Relation to the driver

Today `compile()` stops before emission and says so. When emission lands,
`--check-syntax` keeps its current meaning (frontend only) and full
compilation gains backend flags; the exit-code contract of chapter 3 is
extended, not replaced.
