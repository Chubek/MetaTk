local M = {}

M.VERSION = "0.1.0"

local function fail(message)
  error("lxbind: " .. message, 3)
end

local function is_array(value)
  if type(value) ~= "table" then
    return false
  end
  local count = 0
  for key, _ in pairs(value) do
    if type(key) ~= "number" or key < 1 or key % 1 ~= 0 then
      return false
    end
    count = count + 1
  end
  for index = 1, count do
    if value[index] == nil then
      return false
    end
  end
  return true
end

local function list(value, field)
  if value == nil then
    return {}
  end
  if not is_array(value) then
    fail(field .. " must be an array")
  end
  return value
end

local function require_string(value, field)
  if type(value) ~= "string" or value == "" then
    fail(field .. " must be a non-empty string")
  end
  return value
end

local function copy_options(options)
  local result = {}
  if options == nil then
    return result
  end
  if type(options) ~= "table" then
    fail("options must be a table")
  end
  for key, value in pairs(options) do
    result[key] = value
  end
  return result
end

function M.target(name, options)
  local result = copy_options(options)
  result.kind = "target"
  result.name = require_string(name, "target name")
  result.features = list(result.features, "target.features")
  result.typedefs = list(result.typedefs, "target.typedefs")
  return result
end

function M.typedef(c_type, native_type, options)
  local result = copy_options(options)
  result.kind = "typedef"
  result.c = require_string(c_type, "typedef c type")
  result.native = require_string(native_type, "typedef native type")
  return result
end

function M.param(name, c_type, options)
  local result = copy_options(options)
  result.kind = "param"
  result.name = require_string(name, "param name")
  result.c = require_string(c_type, "param c type")
  return result
end

function M.func(name, options)
  local result = copy_options(options)
  result.kind = "function"
  result.name = require_string(name, "function name")
  result.returns = require_string(result.returns or "void", "function returns")
  result.params = list(result.params, "function.params")
  result.features = list(result.features, "function.features")
  return result
end

M["function"] = M.func

function M.module(name, options)
  local result = copy_options(options)
  result.kind = "module"
  result.name = require_string(name, "module name")
  result.functions = list(result.functions, "module.functions")
  result.typedefs = list(result.typedefs, "module.typedefs")
  return result
end

function M.library(options)
  local result = copy_options(options)
  result.kind = "library"
  result.name = require_string(result.name, "library name")
  result.headers = list(result.headers, "library.headers")
  result.targets = list(result.targets, "library.targets")
  result.typedefs = list(result.typedefs, "library.typedefs")
  result.functions = list(result.functions, "library.functions")
  result.modules = list(result.modules, "library.modules")
  result.stdlib = list(result.stdlib, "library.stdlib")
  result.features = list(result.features, "library.features")
  result.version = result.version or M.VERSION
  return result
end

function M.validate_library(manifest)
  if type(manifest) ~= "table" or manifest.kind ~= "library" then
    fail("manifest must be produced by xbind.library")
  end
  require_string(manifest.name, "library name")
  require_string(manifest.version, "library version")
  local supported = {D=true, Go=true, Java=true, Lua=true, NodeJS=true,
                     OCaml=true, Python=true, Ruby=true, Rust=true}
  local function options(value)
    for _, key in ipairs({"ownership", "range", "encoding", "direction",
        "module", "package", "class", "addon", "library", "extension", "crate"}) do
      if value[key] ~= nil then require_string(value[key], key) end
    end
    if value.nullable ~= nil and type(value.nullable) ~= "boolean" then
      fail("nullable must be a boolean")
    end
  end
  local function strings(value, field)
    for _, item in ipairs(list(value, field)) do require_string(item, field) end
  end
  local function identifier(value)
    require_string(value, "identifier")
    if not value:match("^[A-Za-z_][A-Za-z_0-9]*$") then fail("invalid C identifier: " .. value) end
  end
  local function c_type(value)
    require_string(value, "C type")
    local first = value:find("*", 1, true)
    local base = first and value:sub(1, first - 1) or value
    if not base:match("^[A-Za-z_]") then fail("unsupported C type spelling: " .. value) end
    for word in base:gmatch("%S+") do
      if not word:match("^[A-Za-z_][A-Za-z_0-9]*$") then fail("unsupported C type spelling: " .. value) end
    end
    local tail = first and value:sub(first) or ""
    while tail ~= "" do
      if tail:sub(1, 1) ~= "*" then fail("unsupported C type spelling: " .. value) end
      tail = tail:sub(2):gsub("^%s+", "")
      if tail:sub(1, 5) == "const" and not tail:sub(6, 6):match("[A-Za-z_0-9]") then
        tail = tail:sub(6):gsub("^%s+", "")
      end
    end
  end
  local function typedefs(value)
    local seen = {}
    for _, item in ipairs(list(value, "typedefs")) do
      if type(item) ~= "table" or item.kind ~= "typedef" then fail("invalid typedef") end
      require_string(item.c, "typedef c type")
      c_type(item.c)
      require_string(item.native, "typedef native type")
      if seen[item.c] then fail("duplicate typedef: " .. item.c) end
      seen[item.c] = true
      options(item)
    end
  end
  strings(manifest.headers, "headers")
  for _, header in ipairs(manifest.headers or {}) do
    if header:find('["\n\r\\]') then fail("invalid header path: " .. header) end
  end
  strings(manifest.stdlib, "stdlib")
  strings(manifest.features, "features")
  typedefs(manifest.typedefs)
  if #list(manifest.modules, "modules") ~= 0 then fail("module lowering is not supported; use library.functions") end
  local targets = {}
  for index, target in ipairs(list(manifest.targets, "targets")) do
    if type(target) ~= "table" or target.kind ~= "target" then
      fail("library.targets[" .. index .. "] must be produced by xbind.target")
    end
    require_string(target.name, "target name")
    if not supported[target.name] then fail("unknown target: " .. target.name) end
    if targets[target.name] then fail("duplicate target: " .. target.name) end
    targets[target.name] = true
    options(target)
    typedefs(target.typedefs)
    strings(target.features, "target.features")
  end
  local functions = {}
  for index, fn in ipairs(list(manifest.functions, "functions")) do
    if type(fn) ~= "table" or fn.kind ~= "function" then
      fail("library.functions[" .. index .. "] must be produced by xbind.func")
    end
    identifier(fn.name)
    require_string(fn.returns, "function returns")
    c_type(fn.returns)
    if functions[fn.name] then fail("duplicate function: " .. fn.name) end
    functions[fn.name] = true
    options(fn)
    strings(fn.features, "function.features")
    local params = {}
    for _, param in ipairs(list(fn.params, "params")) do
      if type(param) ~= "table" or param.kind ~= "param" then fail("invalid parameter") end
      identifier(param.name)
      require_string(param.c, "param c type")
      c_type(param.c)
      if param.c == "void" then fail("void parameter: " .. fn.name) end
      if params[param.name] then fail("duplicate parameter: " .. param.name) end
      params[param.name] = true
      options(param)
      if param.nullable ~= nil and not param.c:find("*", 1, true) then
        fail("nullable requires a pointer: " .. param.name)
      end
    end
  end
  return manifest
end

return M
