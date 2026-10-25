-- Expand vector operations for a backend capability set
-- Declarative schema v1. Patterns are documentation-level terms consumed by MVIR-RewriteEngine.
-- The engine must validate guards and must not execute pattern strings as Lua code.
return {
  schema = "metavect.rewrite/v1",
  name = "VectorExpander",
  description = "Expand vector operations for a backend capability set",
  scope = "lowering",
  enabled_by_default = true,
  rules = {
    { id = "scalar-fallback", pattern = "vector_op(args) -> map(scalar_op,args)", guard = "lane order and mask semantics preserved" }
  }
}
