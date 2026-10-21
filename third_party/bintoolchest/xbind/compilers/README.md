# XBind target compilers

Each subdirectory is the planned lowering stage for one target language. A target
compiler consumes normalized XBind IR and emits target glue that uses the matching
DomBind runtime library.

Current target directories:

- `D.c` -> `dombind/domdbind`
- `Go.c` -> `dombind/domgobind`
- `Java.c` -> `dombind/domjavabind`
- `Lua.c` -> `dombind/domluabind`
- `NodeJS.c` -> `dombind/domnodebind`
- `OCaml.c` -> `dombind/domocamlbind`
- `Python.c` -> `dombind/dompybind`
- `Ruby.c` -> `dombind/domrubybind`
- `Rust.c` -> `dombind/domrustbind`

The first backend should implement a small traceable pipeline:

1. accept an XBind IR dump,
2. resolve target type mappings,
3. request DomBind primitives,
4. emit source files plus a target-native smoke test.

Current status: the directories are reserved. `../xbind_codegen.cpp` implements
the JSON spec IR, compiler signature checks, and the working Lua emitter through
DomLuaBind. `../tests/test_codegen.py` compiles and executes hello and self-binding
modules. The other eight emitters are not implemented. Next steps:

1. add header AST/source-span ingestion alongside the exact compiler checks;
2. extend the same IR to additional target emitters;
3. add records, callbacks, buffers and automatic ownership with runtime tests.
