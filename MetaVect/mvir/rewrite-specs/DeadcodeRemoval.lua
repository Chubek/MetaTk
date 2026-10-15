-- Remove unreachable or unused pure computations
-- Declarative schema v1. Patterns are documentation-level terms consumed by MVIR-RewriteEngine.
-- The engine must validate guards and must not execute pattern strings as Lua code.
return {
  schema = "metavect.rewrite/v1",
  name = "DeadcodeRemoval",
  description = "Remove unreachable or unused pure computations",
  scope = "local",
  enabled_by_default = true,
  rules = {
    { id = "unused-pure-let", pattern = "let x = pure(e) in body -> body", guard = "x not free in body; e has no effects" },
{ id = "unreachable-branch", pattern = "if true then a else b -> a", guard = "condition is known boolean constant" }
  }
}
