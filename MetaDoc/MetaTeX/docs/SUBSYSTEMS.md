# Commands, streams, fonts, devices and Lua

These modules extend the native **subset** engine. They do not establish TeX82
or general LaTeX compatibility. `metatex::Engine` and the plugin registration
entry point retain their existing APIs.

## Build and dependencies

The normal build uses the workspace's `third_party/libharu` for PDF, system
FreeType for font inspection, and HarfBuzz for OpenType shaping. LibHaru's
license is checked in its source directory and installed with MetaTeX.
`METATEX_LIBHARU_DIR` overrides its source location. No dependency is downloaded.
PostScript, ditroff, and EPUB use no additional libraries.

Lua 5.3 or newer is found with CMake's `FindLua`. Set
`METATEX_ENABLE_LUA=OFF` when building without Lua development files.
`METATEX_TEST_FONT` selects the scalable Unicode font used by integration tests;
the default searches for the system DejaVu Sans font.

The stream library also builds with `METATEX_TEXSCRAPE_ONLY=ON`. It uses POSIX
descriptors, sockets, shared memory, message queues, and threads. Windows is not
a supported platform for these implementations.

## Command modules

`command/Builtin.cpp` owns the engine's built-in installation. `Builtin.hpp`
provides the sorted command inventory. `Native.hpp` adapts a C++ callback taking
an argument span and returning text or TeX. `Symbol.hpp` inserts literal UTF-8.
`MML.hpp` uses the existing MetaML runtime, and `Extern.hpp` executes
shebang-selected scripts. Native/MetaML/external command registration accepts
an arity from zero through nine and an optional group-local definition flag.

```cpp
#include "command/Native.hpp"
metatex::Engine engine;
metatex::command::registerNative(engine, "join", 2,
    [](std::span<const std::string> args) { return args[0] + args[1]; });
auto document = engine.compile(R"(\join{hello}{world})");
```

`Shell.hpp` distinguishes direct executable/argument invocation (`runProcess`)
from explicit `/bin/sh -c` evaluation (`runShell`). Script arguments are not
concatenated into shell code. Output limits, selectable capture descriptors,
deadlines, process-group cleanup and exit-status checks apply to both.
Commands inserting TeX use the existing default-category `Lexer::scan` helper;
they do not yet capture the engine's mutable category table.

## Readers and sinks

All readers implement `metatex::io::Reader`; binary NUL bytes are preserved.
`readAll(maxBytes)` applies the existing bounded-input contract. A zero-capacity
read returns zero without consuming data.

* `FIFOReader` / `FIFOSink` open an **existing** FIFO, verify its descriptor type,
  and follow blocking POSIX rendezvous semantics. The application creates and
  unlinks the FIFO. EOF follows writer close. Broken-pipe writes throw without
  changing application-wide SIGPIPE handling.
* `SharedMemorySink` buffers a bounded blob and creates an exclusive `0600`
  POSIX shared-memory object on `flush()`. `SharedMemoryReader` copies a bounded
  snapshot. Publication is complete only when `flush()` returns; the application
  must synchronize readers and avoid mutation/resizing during snapshot copying.
  The application calls `shm_unlink`. Existing names are never overwritten.
* `MSGQueueSink` can exclusively create a queue or open an existing one. Messages
  have priority zero and are concatenated by `MSGQueueReader`, retaining partial
  reads. `finish()` sends an empty EOF message. `flush()` does not end the stream.
  Use one producer and consumer per stream. Operations use finite deadlines;
  the application calls `mq_unlink`. Queue creation respects OS resource limits.
* `connectTCP` / `connectUnix` return caller-owned connected stream descriptors.
  `SocketReader` / `SocketSink` can borrow or own them. Reads and writes have
  per-operation deadlines and handle short I/O; `finish()` half-closes output.
  DNS resolution precedes the connection deadline and is synchronous. Socket
  writes suppress SIGPIPE locally. For servers, pass an accepted descriptor.

IPC objects are not unlinked by destructors. Queue/socket EOF must be explicitly
sent, so destructor failures cannot masquerade as successful delivery. Stream
instances require external synchronization if used from multiple threads.

`DiagnosticSink` formats source-positioned diagnostics and counts errors.
`DeviceSink` renders a complete `Document` before writing it to any `Sink` and
rejects documents containing errors. Network writes may deliver a prefix before
an error; they are not transactions.

## Fonts

`FontLoader` returns shared immutable font snapshots with FreeType metadata.
Its bounded weak cache is keyed by canonical path and face index; `clear()` is
required after changing an in-use file. Scalable collection faces are supported.
`FontCatalogue` imports manifests transactionally and selects a family/style.

The UTF-8 manifest format is:

```text
metatex-fonts 1
"body" "/usr/share/fonts/TTF/DejaVuSans.ttf" 0
```

Relative paths resolve against the manifest directory. Blank lines and full-line
`#` comments are accepted. Duplicate IDs, invalid face indices and trailing
tokens are rejected. The format is versioned and round-trips quoted paths.

`FontShaping.hpp` shapes a single directional/script run with HarfBuzz. It returns
glyph IDs, UTF-8 byte clusters, point-based advances/offsets, resolved direction,
and a missing-glyph flag. Language, script, direction, size and OpenType features
are configurable. Paragraph bidi segmentation and font fallback belong to the
caller. Font inspection and shaping use independent library handles and do not
mutate shared snapshots. These APIs do not yet replace the native page layout.

## Output devices

`--list-devices` lists `html`, `pdf`, `text`, `json`, `ps`, `troff-ir`, `epub`.
All consume `Document`. CLI, REPL, `renderDevice`, `DeviceSink`, and the C++
`OutputFormat` API share those devices.

* **PDF:** libHaru owns fonts/resources, PDF objects, cross references and
  serialization. Its low-level page streams retain the native renderer's exact
  vector geometry and deterministic output. The semantic renderer uses base-14
  fonts with limited Latin-1 text; it does not perform Unicode math layout.
  The explicitly called optional `renderCairoPDF` compatibility API remains
  available, but CLI PDF output uses libHaru.
* **PostScript:** DSC Level 2, paginated text, escaped Latin-1 strings, rules,
  explicit page breaks and native vector fills/strokes/curves. Characters outside
  Latin-1 use `?`, matching the device's base-font limitation.
* **TROFF IR:** ditroff `ps` device stream at 72000 units/inch, TR/TB fonts,
  scaled point sizes, absolute page/line positions, Unicode glyph names, rules
  and page breaks. This is text-oriented IR: math retains source and vector
  blocks retain captions. It is accepted by `grops` without a roff macro package.
* **EPUB:** reproducible EPUB 3 stored ZIP, required first uncompressed `mimetype`,
  container, OPF metadata/manifest/spine, XHTML content and navigation, inline
  MathML and SVG. CRCs and ZIP32 size limits are checked. Semantic image references
  remain textual descriptions; external files are not implicitly fetched or
  embedded. Language defaults to English because `Document` has no language field.

The PDF/PostScript/ditroff devices use simple semantic pagination and wrapping,
not TeX's box/glue page builder. Font shaping is exposed separately.

## Lua

```lua
local lmtex = require("lmtex")
local engine = lmtex.new()
local pdf, diagnostics = engine:compile("Hello Lua.", "pdf", "example.tex")
assert(pdf, diagnostics)
local file = assert(io.open("example.pdf", "wb"))
file:write(pdf)
file:close()
engine:close()
```

The module is built at `build/lua/lmtex.so`; add `build/lua/?.so` to `LUA_CPATH`.
Use a Lua interpreter matching the CMake-selected version. `compile` resets
document state; `fragment` preserves definitions. Both return output bytes and
diagnostic text, or `nil` and diagnostic text on failure. Engines use the native
plain subset; dialect plugins are not automatically loaded. GC and explicit
`close()` release ownership. `lmtex.h` describes the C boundary, with no escaping
C++ exceptions. Temporary result buffers remain GC-owned across Lua allocation
errors. The TeX/LaTeX files beside the binding are executable documentation.

## Verification and remaining specification work

`subsystem-modules` checks real FIFO/shared-memory/message-queue/socket transfers,
EOF, bounds, timeouts, peer closure, command IR/HTML/PDF, font metadata/cache,
manifest rollback and shaping. `alternate-devices` validates ZIP/XML, navigation,
determinism, PDF text and independent Ghostscript/grops parsing when available,
and compiles every new template/example in all seven formats. `lua-binding`
checks source compilation, IR/HTML/PDF output, persistent fragments and errors.

The requested `inter/*.midl` and `texpresso/*.qs` files still await the user's
existing language specifications/compiler paths (including TAMNY's role).
No local grammar was authorized. `inter/CMakeLists.txt` therefore also awaits
the actual schema compiler's build contract. The listed `build/CMakeFiles/...`
paths are generated CMake directories, not source implementations.
