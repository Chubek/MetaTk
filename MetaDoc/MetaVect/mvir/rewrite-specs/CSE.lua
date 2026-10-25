-- Common subexpression elimination
-- Declarative schema v1. Patterns are documentation-level terms consumed by MVIR-RewriteEngine.
-- The engine must validate guards and must not execute pattern strings as Lua code.
return {
  schema = "metavect.rewrite/v1",
  name = "CSE",
  description = "Common subexpression elimination",
  scope = "global",
  enabled_by_default = true,
  rules = {
    { id = "reuse-pure", pattern = "let t = pure(e); ... pure(e) -> reuse t", guard = "same scope and identical type/effect" }
  }
}
