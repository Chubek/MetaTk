-- Remove stores overwritten before any read
return {
  schema = "metavect.rewrite/v1",
  name = "DeadStoreElimination",
  description = "Remove stores overwritten before any read",
  scope = "block",
  enabled_by_default = false,
  rules = {
    { id = "overwritten-store", pattern = "store(p,a); store(p,b) -> store(p,b)", guard = "same location, no intervening read or observable barrier" }
  }
}
