# Lua Integration

When Lua is found, CMake builds `larchld.so`. `require("larchld")` returns a module with `new`; linker userdata supports `add`, `link`, `symbol`, `write_elf`, and `error`. Lua owns the userdata while ArchLD owns copied input state.

See the neighboring chapters for the surrounding workflow and the project README for build commands and current limitations.

A complete script looks like this:

```lua
local larchld = require "larchld"
local ld = assert(larchld.new(nil, "ELF64"))
assert(ld:add("entry.o"))
assert(ld:add("run.o"))
assert(ld:link())
local entry = assert(ld:symbol("_start"))
assert(ld:write_elf("linked", entry))
```

The module uses the catalogue and target defaults selected by ArchLD. Pass a
catalogue path and an explicit base when embedding it in a tool that needs
reproducible address assignment. A module compiled for one Lua ABI must be
loaded by the matching Lua major and minor ABI.
