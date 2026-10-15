-- Precompute constant geometry constructors
return {
  schema = "metavect.rewrite/v1",
  name = "ConstantGeometry",
  description = "Precompute constant geometry constructors",
  scope = "local",
  enabled_by_default = false,
  rules = {
    { id = "constant-transform", pattern = "transform(const(M), const(P)) -> const(M*P)", guard = "matrix convention and numeric semantics fixed" }
  }
}
