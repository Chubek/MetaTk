-- Loop-invariant code motion
-- Declarative schema v1. Patterns are documentation-level terms consumed by MVIR-RewriteEngine.
-- The engine must validate guards and must not execute pattern strings as Lua code.
return {
  schema = "metavect.rewrite/v1",
  name = "LICM",
  description = "Loop-invariant code motion",
  scope = "loop",
  enabled_by_default = true,
  rules = {
    { id = "hoist-pure", pattern = "loop(body(pure(e))) -> let t=pure(e) in loop(body(t))", guard = "e invariant, total, no memory effects" }
  }
}
