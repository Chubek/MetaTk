-- Simplify control-flow graph branches and joins
return {
  schema = "metavect.rewrite/v1",
  name = "CFGSimplification",
  description = "Simplify control-flow graph branches and joins",
  scope = "function",
  enabled_by_default = false,
  rules = {
    { id = "same-successor", pattern = "branch(c, b, b) -> b", guard = "branch condition has no observable effects" }
  }
}
