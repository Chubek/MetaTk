-- Fuse compatible map/vector passes
return {
  schema = "metavect.rewrite/v1",
  name = "VectorFusion",
  description = "Fuse compatible map/vector passes",
  scope = "local",
  enabled_by_default = false,
  rules = {
    { id = "map-map", pattern = "map(g, map(f, xs)) -> map(compose(g,f), xs)", guard = "both functions pure; lane semantics preserved" }
  }
}
