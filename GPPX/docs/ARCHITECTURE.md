# Architecture

```text
      file / standard input / caller buffer
                    |
       filesystem + optional Reader plugin
                    |
      Engine::processSource() — line stream
                    |
           recognize active directive marker
           /                     \
     directives                normal text
    condition stack             | 
    include resolver         lexical macro expansion
    block controls           (comments, strings, recursion guard)
    syntax controls             |
    plugin handlers           regex + plugin rewriters
           \                    /
            bounded output accumulator
                    |
              caller / file / Sink
```

The engine is a C++17 PImpl class (`gppx::Engine`). `process()` accepts a memory buffer and path label; `processFile()` resolves a filesystem entry. Both return the text output or throw `gppx::Error`. Configured include paths and installed handlers persist across requests on an Engine instance. `processSource()` has a local conditional stack and lexical comment state; nested files and block evaluations receive their own state. Shared macro definitions, include bookkeeping and syntax options live on the engine.

The C ABI is declared in `include/GPPX.h`. A `GPPX_State*` is an opaque pointer into the Engine; `gppx_create()` allocates an owning engine, while `Engine::state()` exposes a borrowed pointer for extensions. `gppx_destroy` only destroys owned states. Plugin extensions register using the host function table and are invoked through the same engine. An extension callback must allocate replacement text with `malloc` or a compatible allocation routine; the host frees it after copying.

Plugin libraries are loaded with `dlopen` or `LoadLibrary`. They export `gppx_plugin_init`, which receives `GPPX_HostAPI` and a state pointer. Plugin libraries stay resident while their callbacks can be used; they are not hot-unloaded. The host callbacks have C linkage and ABI version 1; C++ header interfaces are a convenience and not a cross-compiler binary plugin ABI.

## Extension categories

| Category | C hook | How it is used |
|---|---|---|
| Directive | `register_directive` | new `#NAME ARGS` directive |
| Macro | `register_macro` | callback for `NAME(args)` |
| Rewriter | `register_rewriter` | post-expansion line filter |
| Reader | `register_reader` | source read by file extension |
| Sink | `register_sink` | `#sink NAME CONTENT` output dispatch |
| Embed | `register_embed` | `#embed "FILE" FORMAT` encoder |
| Syntax | `register_syntax` | `#syntax CUSTOM_OPTION VALUE` |
| Pattern matcher | `register_pmatch` | `#ifmatch ENGINE PATTERN => SUBJECT` |

## Ownership, limits and state

Ownership: use normal RAII for Engine; `gppx_free()` for buffers returned by `gppx_process` and plugin-allocated callback buffers. Plugins must not retain the *host function table pointer*; copy the needed function pointers during initialization. Plugin callbacks receive their original opaque `userdata` pointer. Thread safety is at the Engine level: **do not invoke the same Engine concurrently** (registering handlers or running processes mutates state). Separate Engine instances may be used independently.

Default limits: macro expansion depth 64, include depth 64, output 16 MiB, repeat/for iteration bound 100000. These are limits on normal processing, not protection against malicious native plugins or adversarial regex worst cases.

## Implemented C extension helpers (1.0.1)

The engine (`GPPX.cpp`) shares its versioned C ABI with the C11 helper module
(`GPPX.c`). The latter is built directly into `libgppx`, not compiled into
plugins by their authors. The category headers `GPPX-{Directive,Macro,Syntax,
Reader,Sink,Embed,Rewriter,PMatch}.h` each declare functions corresponding to
that capability; `<GPPX-all.h>` exposes them all to installed C clients.

The driver layer now uses `driver/loader.cpp` for deterministic, nonrecursive
loading of plugin directories, `driver/config.cpp` for its documented YAML
subset and `driver/repl.cpp` for multiline/persistent interactive operation.
The registry YAML files are descriptive catalogues rather than executable
registrations: GPPX core code registers builtin behavior, and each plugin
registers callbacks from `gppx_plugin_init`.
