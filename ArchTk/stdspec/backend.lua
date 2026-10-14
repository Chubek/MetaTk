-- Backend extension for the existing DomSymAsCC DSL. No I/O or JSON parser
-- is required by DomMune: generated modules contain serialized JSON.
local M = {}

function M.new(arch)
    local symas = require("dsl").new()
    local emit = symas.emit
    local metadata = require("backend." .. arch)
    symas.emit = function()
        local document = emit()
        if string.sub(document, -1) ~= "}" then
            error("backend: expected an object from dsl.emit()")
        end
        return string.sub(document, 1, #document - 1) .. ',"backend":' .. metadata .. "}"
    end
    return symas
end

return M
