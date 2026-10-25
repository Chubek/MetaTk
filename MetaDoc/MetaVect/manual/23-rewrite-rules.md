# Chapter 23 — Rewrite Rules

**Status:** schema implemented and catalogued; engine pending
(`mvir/rewrite-specs/`, `mvir/rewrite-specs/README.md`).

## Schema

Each Lua spec returns a plain table: `schema`, `name`, `description`,
`scope`, `enabled_by_default`, and an ordered `rules` array. Each rule has
a stable `id`, a human-readable `pattern`, and a `guard`. These strings are
declarative metadata — never Lua fragments, never passed to `load`.

Validate with `luac -p mvir/rewrite-specs/*.lua`, which checks syntax
without executing anything.

## Required engine behavior

Validate schema and unique rule IDs first. Apply rules in deterministic
order under a configurable fuel limit. Require purity/effect and type
guards before transforming. Record provenance for debug output. Stop with
a diagnostic on rewrite cycles instead of looping. Preserve NaN,
infinities, signed zero, and rounding unless fast-math is explicitly
enabled. Treat profile data as a hint that must never change meaning.

## Rule quality checklist

State the exact pattern and replacement; state side conditions (types,
purity, dimensions, overflow, effects); add positive and negative tests;
explain code-size growth and ordering; keep transformations local unless a
pass documents wider scope. The current catalog (constant folding,
inlining, fusion, CSE, LICM, and two dozen more) is an initial collection,
not proof the engine supports every pattern.
