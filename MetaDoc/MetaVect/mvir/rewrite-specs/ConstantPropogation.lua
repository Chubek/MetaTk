-- Propagate known immutable constants
-- Declarative schema v1. Patterns are documentation-level terms consumed by MVIR-RewriteEngine.
-- The engine must validate guards and must not execute pattern strings as Lua code.
return {
  schema = "metavect.rewrite/v1",
  name = "ConstantPropogation",
  description = "Propagate known immutable constants",
  scope = "local",
  enabled_by_default = true,
  rules = {
    { id = "let-constant", pattern = "let x = const(c) in use(x) -> let x = const(c) in use(const(c))", guard = "only until a rebinding/effect boundary" }
  }
}
