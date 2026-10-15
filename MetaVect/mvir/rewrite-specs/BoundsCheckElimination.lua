-- Eliminate redundant proven-safe bounds checks
return {
  schema = "metavect.rewrite/v1",
  name = "BoundsCheckElimination",
  description = "Eliminate redundant proven-safe bounds checks",
  scope = "local",
  enabled_by_default = false,
  rules = {
    { id = "in-range-index", pattern = "check_index(a, i) -> i", guard = "range(i) proven inside array bounds and array length unchanged" }
  }
}
