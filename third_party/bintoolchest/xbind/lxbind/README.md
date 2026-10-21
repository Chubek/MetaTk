# lxbind

`lxbind.lua` is the Lua-side helper module for XBind specs. Specs are ordinary
Lua files, usually named `<library>.xbind`, that return a manifest built from the
helper constructors:

- `xbind.library { ... }`
- `xbind.target("Python", { ... })`
- `xbind.typedef("const char *", "str", { ... })`
- `xbind.param("name", "int", { ... })`
- `xbind.func("symbol", { returns = "int", params = { ... } })`
- `xbind.module("name", { functions = { ... } })`

The helper performs only structural validation. The driver is responsible for
executing specs in a controlled Lua environment, resolving headers, applying type
maps, and rejecting target-specific mistakes.

Supported target names in the current scaffold are `D`, `Go`, `Java`, `Lua`,
`NodeJS`, `OCaml`, `Python`, `Ruby`, and `Rust`. Specs in this repository list
all of them. Target option tables are preserved for future lowering but are not
interpreted by `lxbind.lua`.

`validate_library` checks that a value was produced by `xbind.library`, that the
library name is non-empty, and that target/function arrays contain constructor
produced tables. It does not parse C headers, verify function declarations, or
resolve type maps.

`qamrpp_xbind.cpp` builds `libqamrpp_xbind.so`, a native QaMRpp module exposing the
same constructor vocabulary for hosts that want the XBind helpers without loading
the Lua source file.
