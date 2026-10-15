# MVIR rewrite specification schema

Each Lua file returns a plain table with `schema`, `name`, `description`, `scope`,
`enabled_by_default`, and an ordered `rules` array. A rule contains a stable `id`,
a human-readable `pattern`, and a `guard`. These strings are declarative metadata;
they are not Lua source fragments and must never be passed to `load`, `loadstring`,
or an equivalent evaluator.

## Required optimizer behavior
- Validate schema and unique rule IDs before running a spec.
- Apply rules in deterministic order with a configurable rewrite-fuel limit.
- Require purity/effect and type guards before applying transformations.
- Record rule provenance for debug output and regression tests.
- Detect rewrite cycles and stop with a diagnostic rather than looping forever.
- Preserve floating-point corner cases unless fast-math is explicitly enabled.
- Treat profile data as a hint only; it must not affect program meaning.

## Rule quality checklist
1. State the exact pattern and replacement.
2. State side conditions (types, purity, dimensions, overflow, effects).
3. Add positive and negative tests.
4. Explain code-size growth and ordering constraints.
5. Keep transformations local unless a pass explicitly documents whole-function/module scope.

The current rules are an initial catalog, not proof that the engine supports every pattern.
