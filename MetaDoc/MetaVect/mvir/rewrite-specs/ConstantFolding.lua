-- Evaluate constant primitive operations
-- Declarative schema v1. Patterns are documentation-level terms consumed by MVIR-RewriteEngine.
-- The engine must validate guards and must not execute pattern strings as Lua code.
return {
  schema = "metavect.rewrite/v1",
  name = "ConstantFolding",
  description = "Evaluate constant primitive operations",
  scope = "local",
  enabled_by_default = true,
  rules = {
    { id = "fold-add", pattern = "add(const(a), const(b)) -> const(a+b)", guard = "no overflow/rounding semantic change" },
{ id = "fold-neg", pattern = "neg(const(a)) -> const(-a)", guard = "preserve signed zero and NaN" }
  }
}
