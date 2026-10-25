-- Specialize repeated dynamic dispatch sites
return {
  schema = "metavect.rewrite/v1",
  name = "InlineCache",
  description = "Specialize repeated dynamic dispatch sites",
  scope = "module",
  enabled_by_default = false,
  rules = {
    { id = "monomorphic-call", pattern = "call(site, target) -> direct_call(target)", guard = "guard target identity and retain generic fallback" }
  }
}
