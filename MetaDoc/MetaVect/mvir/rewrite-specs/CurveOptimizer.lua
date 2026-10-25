-- Simplify vector curve geometry
-- Declarative schema v1. Patterns are documentation-level terms consumed by MVIR-RewriteEngine.
-- The engine must validate guards and must not execute pattern strings as Lua code.
return {
  schema = "metavect.rewrite/v1",
  name = "CurveOptimizer",
  description = "Simplify vector curve geometry",
  scope = "local",
  enabled_by_default = true,
  rules = {
    { id = "zero-length-line", pattern = "line(p, p) -> point(p)", guard = "only if line caps/markers are not observable" },
{ id = "collinear-cubic", pattern = "cubic(p0,p1,p2,p3) -> line(p0,p3)", guard = "control points proven collinear and monotone" }
  }
}
