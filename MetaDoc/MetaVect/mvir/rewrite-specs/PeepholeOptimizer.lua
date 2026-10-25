-- Local instruction/term cleanup
-- Declarative schema v1. Patterns are documentation-level terms consumed by MVIR-RewriteEngine.
-- The engine must validate guards and must not execute pattern strings as Lua code.
return {
  schema = "metavect.rewrite/v1",
  name = "PeepholeOptimizer",
  description = "Local instruction/term cleanup",
  scope = "local",
  enabled_by_default = true,
  rules = {
    { id = "identity-cast", pattern = "cast_T(x) -> x", guard = "source type is already T" },
{ id = "nested-negation", pattern = "neg(neg(x)) -> x", guard = "numeric type semantics permit it" }
  }
}
