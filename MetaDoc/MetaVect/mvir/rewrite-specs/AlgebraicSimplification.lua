-- Simplify safe algebraic identities
-- Declarative schema v1. Patterns are documentation-level terms consumed by MVIR-RewriteEngine.
-- The engine must validate guards and must not execute pattern strings as Lua code.
return {
  schema = "metavect.rewrite/v1",
  name = "AlgebraicSimplification",
  description = "Simplify safe algebraic identities",
  scope = "local",
  enabled_by_default = true,
  rules = {
    { id = "add-zero", pattern = "add(x, 0) -> x", guard = "integer-or-exact arithmetic only" },
{ id = "mul-one", pattern = "mul(x, 1) -> x", guard = "numeric" },
{ id = "sub-self", pattern = "sub(x, x) -> 0", guard = "only when x is pure and evaluation is total" }
  }
}
