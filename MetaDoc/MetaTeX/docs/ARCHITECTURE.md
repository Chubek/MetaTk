# MetaTeX implementation architecture

## Functional pipeline

```text
UTF-8 input / include
        |
     Lexer::next                  core/Lexer.cpp (incremental current catcodes)
        |
   Token deque (with source coordinates)
        |
   Engine::run / macro scopes    core/Engine.cpp
        |
  handler registry + plugins     plugins/latex, plugins/nekotex, plugins/metavect
        |
    Document IR                   Block + Span + immutable vector scenes
        |
       +-- HTML + MathML          device/HTML.cpp + core/Math.cpp
       +-- JSON semantic IR       device/JSON.cpp
       +-- Text                   device/HTML.cpp
       +-- PDF                    device/CairoPango.cpp (optional)
       +-- PDF fallback           device/PDF.cpp
```

## Plugin ABI

`metatex_plugin_id()` and `metatex_register_plugin(metatex::Engine*)` are
C linkage entry points (`include/MetaTeX-Plugin.h`). Plugins share the
C++ public `Engine` ABI with the host and must use its shared library.
The CLI uses the workspace's dynalo library for loading, typed symbol lookup,
and lifetime management; native plugin suffixes come from CMake.

To implement a new command:

```cpp
extern "C" const char* metatex_plugin_id() { return "mydialect"; }
extern "C" void metatex_register_plugin(metatex::Engine* engine) {
    engine->registerCommand("greet", [](metatex::Engine& e) {
        e.addText("Hello, ");
        e.execute(e.readArgument());
    });
}
```

For a built-in environment, call `registerEnvironment(name, begin, end)`.
Document-defined environments use `defineEnvironment`, whose definitions follow
group scope. Closing handlers are captured on entry. `executeGrouped` runs a
token argument in a local group; both execution methods protect caller groups
and recover unclosed local environments. `beginBlockContext`/`endBlockContext`
retain semantic roles across paragraphs; `codeBlock` retains literal text and
language metadata. The corresponding new fields are appended to `Block`.
The callback may start/flush document blocks. `execute()` recursively
processes an argument through the normal token and macro engine.
Nested execution owns a separate token queue, so argument scanners cannot read
past their execution frame. Plugin libraries must outlive their handlers and
any objects whose destructor code lives in the library. The CLI unloads libraries
after the document and engine are destroyed.

`compile()` resets document state and restores the externally registered handler
and environment tables on success or exception. Definitions made by a document
do not leak into the next compilation. Macro definitions and font styles obey
group boundaries; this is still not full TeX group-local state.

`compileFragment()` is the incremental entry point used by the replxx REPL.
It preserves definitions, metadata and counters, and returns only newly emitted
blocks. Group-local native handlers share shadowing rules with macros.
The REPL accumulates those blocks for output without replaying source.

`core/Scripting.cpp` defines MetaML and shebang-backed commands. MetaML embeds
the actual `MetaML.hpp` and all implemented metalib installers; external scripts
use `posix_spawn` with an argv vector and a captured descriptor. Both paths buffer
successful output before tokenizing it into the current engine queue. See
[SCRIPTING.md](SCRIPTING.md) for the command and process contracts.

Vector blocks contain an immutable MetaVect scene and requested display width.
The plugin parses `.mvs` artifacts once through the shared MetaVect API; all
devices consume typed paths. See [METAVECT.md](METAVECT.md).

## Deliberate design limitations

The lexer scans live source incrementally using current category codes. Stored
tokens retain their original categories. Literal readers bypass tokenization
only for live-source tokens, maintain physical positions and share the bounded
source buffer. Full TeX input normalization and Unicode category tables remain
incomplete.

The current layout model is a high-level sequence of blocks and styled spans,
not TeX's box/glue/penalty/horizontal and vertical lists. A true TeX engine
needs horizontal/vertical/math modes, metric-driven glyph shaping, line breaks,
page-building, alignments, insertions and output routines. PDF fallback metrics
are approximate: users needing correct Unicode typography should enable
PangoCairo. Production-grade mathematical layout requires an OpenType MATH
engine, not the current MathML conversion alone.

Advanced `inter/*.midl` and TeXpresso scaffolds remain incomplete. Implemented
reader, sink, font, Lua and alternate-device APIs are described in
[SUBSYSTEMS.md](SUBSYSTEMS.md).

## Security and limits

The engine reads local `\input` files. Reads/source buffers are limited to 16 MiB;
scanning, token queues, macro replacements and total consumed tokens are bounded
at 1.5 million. Group nesting is limited to 1,023 open groups, command execution to 128 frames, includes to
16 levels, and math parser recursion to 128 frames. Invalid UTF-8 is rejected.
Included files resolve paths relative to their containing source file, with
file/line diagnostics and file-local `\endinput`. Filesystem access is not sandboxed.
HTML escapes all user text and restricts clickable links to HTTP(S) or local
fragments. Filesystem I/O does not invoke an external shell.
