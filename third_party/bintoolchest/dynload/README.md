# Tourchstone dynload

`tourchstone/dynload` is a native dynamic-loader wrapper with qBFD-backed object
inspection. It is intended to cover the small-loader use cases of Dynalo and
libltdl while also exposing metadata those libraries do not model: static and
dynamic symbol tables, sections, relocations, dependencies, address-to-file
mapping, and a promoted-symbol environment handle.

## Build

```sh
cmake -S libstone/tourchstone/dynload -B build-dynload
cmake --build build-dynload --parallel
ctest --test-dir build-dynload --output-on-failure
```

The aggregate Tourchstone build enables dynload by default:

```sh
cmake -S libstone/tourchstone -B build-tourchstone \
  -DTOURCHSTONE_BUILD_DYNLOAD=ON
```

## C API sketch

```c
tdyn_env_t *env = NULL;
tdyn_library_t *lib = NULL;
void *sym = NULL;

tdyn_env_create(&env);
tdyn_env_open(env, "libplugin.so", TDYN_OPEN_NOW | TDYN_OPEN_LOCAL, &lib);
tdyn_library_symbol(lib, "plugin_init", &sym);
tdyn_env_promote(env, lib, TDYN_SYMBOL_TABLE_DYNAMIC);
tdyn_env_symbol(env, "plugin_init", &sym, NULL);
tdyn_env_destroy(env);
```

`tdyn_library_*_info` functions return qBFD-derived metadata. Returned strings
are borrowed from the library handle and remain valid until it is closed or its
owning environment is destroyed.

`tdyn_env_new` and `tdyn_library_load` are pointer-return convenience functions
for bindings: NULL signals failure and `tdyn_last_error` provides the diagnostic.
Close standalone libraries once with `tdyn_library_close`. Closing an
environment-owned library removes it and its promoted symbols; all aliases and
metadata pointers become invalid. Promotion requires membership in that same
environment, preventing stale symbols from a separately owned library.

Inspect-only opens require successful qBFD inspection. Unknown flags and mutually
exclusive lazy/now or local/global flags are rejected. Failed symbol lookups
clear outputs. POSIX nodelete handles still release their acquired loader
reference; Windows nodelete pins the module explicitly.

## Tool

`tdynload` opens a library, inspects it, and resolves symbols:

```sh
tdynload --inspect --needed ./libplugin.so
tdynload --symbols ./libplugin.so
tdynload --promote --env-symbol plugin_init ./libplugin.so
tdynload --self --resolve malloc
```

Use `-L DIR` to add search paths and `--default-paths` to import the platform
path environment (`LD_LIBRARY_PATH`, `DYLD_LIBRARY_PATH`, or `PATH`).

## XBind spec

`DynLOAD.xbind` describes the string/int/handle query subset for all XBind
backends: D, Go, Java, Lua, NodeJS, OCaml, Python, Ruby, and Rust. APIs that
return record structs (`tdyn_symbol_info_t`, `tdyn_section_info_t`,
`tdyn_relocation_info_t`) or require out-pointers remain deferred until XBind IR
supports records and out-parameter lowering.
