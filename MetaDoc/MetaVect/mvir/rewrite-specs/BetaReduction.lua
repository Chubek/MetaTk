-- Beta reduction of lambda applications
return {
  schema = "metavect.rewrite/v1",
  name = "BetaReduction",
  description = "Beta reduction of lambda applications",
  scope = "local",
  enabled_by_default = false,
  rules = {
    { id = "beta", pattern = "apply(lambda(x, body), arg) -> substitute(body, x, arg)", guard = "capture-avoiding substitution; preserve evaluation order" }
  }
}
