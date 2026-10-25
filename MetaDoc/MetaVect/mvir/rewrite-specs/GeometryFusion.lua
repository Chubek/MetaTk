-- Fuse compatible vector geometry operations
return {
  schema = "metavect.rewrite/v1",
  name = "GeometryFusion",
  description = "Fuse compatible vector geometry operations",
  scope = "local",
  enabled_by_default = false,
  rules = {
    { id = "transform-chain", pattern = "transform(A, transform(B, p)) -> transform(compose(A,B), p)", guard = "composition order matches column/row convention" }
  }
}
