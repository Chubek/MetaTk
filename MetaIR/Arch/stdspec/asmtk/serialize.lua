-- Serializer for architecture data (no I/O or external JSON dependency).
local function json(value)
    if type(value) == "string" then
        return '"' .. value:gsub('[%z\1-\31\\"]', function(c)
            return c == '\\' and '\\\\' or c == '"' and '\\"' or string.format('\\u%04x', c:byte())
        end) .. '"'
    end
    if type(value) ~= "table" then return tostring(value) end
    local out = {}
    if #value > 0 or next(value) == nil then
        for _, item in ipairs(value) do out[#out + 1] = json(item) end
        return "[" .. table.concat(out, ",") .. "]"
    end
    local keys = {}
    for key in pairs(value) do keys[#keys + 1] = key end
    table.sort(keys)
    for _, key in ipairs(keys) do out[#out + 1] = json(key) .. ":" .. json(value[key]) end
    return "{" .. table.concat(out, ",") .. "}"
end
return json
