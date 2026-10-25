-- Mark eligible self-tail calls
return {
  schema = "metavect.rewrite/v1",
  name = "TailCallOptimization",
  description = "Mark eligible self-tail calls",
  scope = "function",
  enabled_by_default = false,
  rules = {
    { id = "self-tail-call", pattern = "return apply(f,args) -> tail_apply(f,args)", guard = "callee is current function and no work remains after call" }
  }
}
