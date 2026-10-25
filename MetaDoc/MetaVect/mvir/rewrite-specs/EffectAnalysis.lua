-- Annotate pure and effectful terms for safe rewrites
return {
  schema = "metavect.rewrite/v1",
  name = "EffectAnalysis",
  description = "Annotate pure and effectful terms for safe rewrites",
  scope = "function",
  enabled_by_default = false,
  rules = {
    { id = "pure-call", pattern = "call(f,args) -> pure_call(f,args)", guard = "callee effect summary proves purity" }
  }
}
