-- Remove redundant lambda wrappers
return {
  schema = "metavect.rewrite/v1",
  name = "EtaReduction",
  description = "Remove redundant lambda wrappers",
  scope = "local",
  enabled_by_default = false,
  rules = {
    { id = "eta", pattern = "lambda(x, apply(f, x)) -> f", guard = "x not free in f; function type preserved" }
  }
}
