-- Compatibility entry point: lua build.lua [pdf|check|examples|d-check|clean]
local target = arg[1] or "pdf"
local allowed = {pdf=true, check=true, examples=true, ["d-check"]=true, clean=true,
  assemble=true, markdown=true, listings=true, probes=true, verify=true, distclean=true}
assert(allowed[target], "unknown build target")
local script = debug.getinfo(1, "S").source:sub(2)
local directory = script:match("^(.*[/\\])") or "./"
local quoted = "'" .. directory:gsub("'", "'\\''") .. "'"
local ok, _, status = os.execute("make -C " .. quoted .. " " .. target)
os.exit(ok and 0 or (status or 1))
