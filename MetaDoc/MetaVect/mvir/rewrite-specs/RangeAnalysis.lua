-- Propagate proven scalar ranges
return {
  schema = "metavect.rewrite/v1",
  name = "RangeAnalysis",
  description = "Propagate proven scalar ranges",
  scope = "global",
  enabled_by_default = false,
  rules = {
    { id = "bounded-compare", pattern = "x < c -> true", guard = "range(x).max < c and integer types match" }
  }
}
