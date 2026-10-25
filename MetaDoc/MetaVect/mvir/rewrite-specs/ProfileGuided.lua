-- Use profile metadata to choose safe optimization candidates
-- Declarative schema v1. Patterns are documentation-level terms consumed by MVIR-RewriteEngine.
-- The engine must validate guards and must not execute pattern strings as Lua code.
return {
  schema = "metavect.rewrite/v1",
  name = "ProfileGuided",
  description = "Use profile metadata to choose safe optimization candidates",
  scope = "module",
  enabled_by_default = true,
  rules = {
    { id = "hot-inline", pattern = "hot(apply(f,x)) -> inline candidate", guard = "profile data is advisory; semantics unchanged" }
  }
}
