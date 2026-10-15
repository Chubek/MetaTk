-- Reference schema helpers for rewrite specs. This module deliberately does not evaluate patterns.
local M = {}
M.schema = "metavect.rewrite/v1"
function M.validate(spec)
  if type(spec) ~= "table" then return false, "spec must return a table" end
  if spec.schema ~= M.schema then return false, "unsupported rewrite schema" end
  if type(spec.name) ~= "string" or spec.name == "" then return false, "missing spec name" end
  if type(spec.rules) ~= "table" then return false, "rules must be an array" end
  local seen = {}
  for i, rule in ipairs(spec.rules) do
    if type(rule.id) ~= "string" or rule.id == "" then return false, "rule " .. i .. " has no id" end
    if seen[rule.id] then return false, "duplicate rule id: " .. rule.id end
    seen[rule.id] = true
    if type(rule.pattern) ~= "string" or type(rule.guard) ~= "string" then
      return false, "rule " .. rule.id .. " needs pattern and guard strings"
    end
  end
  return true
end
return M
