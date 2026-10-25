-- Replace aliases with their source values
return {
  schema = "metavect.rewrite/v1",
  name = "CopyPropagation",
  description = "Replace aliases with their source values",
  scope = "local",
  enabled_by_default = false,
  rules = {
    { id = "copy", pattern = "let y = x in use(y) -> let y = x in use(x)", guard = "x immutable and no intervening write" }
  }
}
