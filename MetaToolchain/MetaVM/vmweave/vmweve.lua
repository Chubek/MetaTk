-- Historical filename retained. This is an optional assembly authoring helper,
-- not a replacement for the MetaML-based VMWeave evaluator.
local M = {}
function M.new()
  local lines = {}
  return {
    emit = function(_, line)
      assert(type(line) == 'string' and not line:find('\0'), 'invalid assembly line')
      lines[#lines + 1] = line
    end,
    source = function() return table.concat(lines, '\n') .. '\n' end
  }
end
return M
