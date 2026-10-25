-- Optimize small linear algebra expressions
-- Declarative schema v1. Patterns are documentation-level terms consumed by MVIR-RewriteEngine.
-- The engine must validate guards and must not execute pattern strings as Lua code.
return {
  schema = "metavect.rewrite/v1",
  name = "LinalgOptimizer",
  description = "Optimize small linear algebra expressions",
  scope = "local",
  enabled_by_default = true,
  rules = {
    { id = "matrix-identity", pattern = "matmul(A, identity(n)) -> A", guard = "dimension and numeric semantics match" },
{ id = "transpose-transpose", pattern = "transpose(transpose(A)) -> A", guard = "shape preserved" }
  }
}
