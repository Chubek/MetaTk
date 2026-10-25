-- Inline small eligible functions
-- Declarative schema v1. Patterns are documentation-level terms consumed by MVIR-RewriteEngine.
-- The engine must validate guards and must not execute pattern strings as Lua code.
return {
  schema = "metavect.rewrite/v1",
  name = "FunctionInliner",
  description = "Inline small eligible functions",
  scope = "local",
  enabled_by_default = true,
  rules = {
    { id = "inline-small", pattern = "apply(lambda(x, body), arg) -> substitute(body, x, arg)", guard = "capture-avoiding substitution; respect size budget" }
  }
}
