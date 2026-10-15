-- Unroll small statically bounded loops
return {
  schema = "metavect.rewrite/v1",
  name = "LoopUnrolling",
  description = "Unroll small statically bounded loops",
  scope = "loop",
  enabled_by_default = false,
  rules = {
    { id = "small-counted-loop", pattern = "loop(i, 0, N, body) -> sequence(body[0..N-1])", guard = "N is constant and below configured unroll limit" }
  }
}
