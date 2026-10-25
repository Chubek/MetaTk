# 2. Architecture and data flow

The canonical IR is defined by [`lua/metail/ir.lua`](../lua/metail/ir.lua). Its
module contains functions; each function contains parameters and an ordered
statement list. Expression nodes have tags `int`, `var`, `op`, or `call`.

The input paths converge on this representation:

```text
.mi text -> parser.program ------------------------------+
C descriptors -> native table construction -------------+-> canonical Lua IR
C++/DSLtk values -> C descriptors -> table construction --+
```

Validation checks structural shape, identifiers, operator arities, duplicate
functions/parameters/labels, and branch targets. Analyses and transformations then
operate on the canonical tables. The emitter produces text; the structured bridge
can instead copy the resulting IR into independently owned C records.

`MetaIL.c` contains the C bridge and protects Lua calls with `lua_pcallk`.
`MetaIL.hpp` provides owning C++ values and lowers DSLtk AST nodes to C descriptors.
`lua/metail/api.lua` shares operations with the CLI. `lua/lkopt.lua` loads and
validates kernels. `lua/lkopt.c` implements native integer evaluation and scanning.

The runtime is the actual Lua 5.4 shared library. `lua/lua_compat.h` only declares
the ABI functions used by this project. MetaNum supplies executable statistics
through a C++ binding; its upstream catalogs also include metadata-only entries.

Keeping these boundaries clear matters when extending the system: a new expression
kind needs agreement among validation, conversion, parsing, emission, and passes.

[Contents](README.md) · [Next: Building](03-building.md)
