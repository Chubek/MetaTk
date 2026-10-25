-- Replace expensive operations with cheaper equivalent forms
-- Declarative schema v1. Patterns are documentation-level terms consumed by MVIR-RewriteEngine.
-- The engine must validate guards and must not execute pattern strings as Lua code.
return {
  schema = "metavect.rewrite/v1",
  name = "StrengthReduction",
  description = "Replace expensive operations with cheaper equivalent forms",
  scope = "local",
  enabled_by_default = true,
  rules = {
    { id = "mul-two", pattern = "mul(x, 2) -> add(x, x)", guard = "integer semantics without overflow differences" },
{ id = "pow-two", pattern = "pow(x, 2) -> mul(x, x)", guard = "floating-point reassociation disabled" }
  }
}
