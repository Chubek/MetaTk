# metalib layers

`MetaML.hpp` and `MetaML.d` own parsing, checking, evaluation, sessions and core
primitives. `metalib` adds language modules and host services. `metaml-cli`
connects a session to command-line arguments, files, standard streams and a REPL.

## Language library

`prelude/mod.ml` imports the portable modules (`core`, `int`, `float`, `string`,
`list`, `option`, `result`, `io`). Load it with `Session.load_stdlib(directory)`.
Each module's `test.ml` is exercised by `scripts/run_tests.py`.

Additional modules use `mod.mml`. Both runtimes resolve `import "name"` to
`name/mod.mml` (and continue to support `.ml` files). Import these modules
explicitly after loading the prelude. Native modules run in both dialects;
modules backed by host services require their installer first.

## Extension modules

| Directory / module | API and representation | Required host installer |
| --- | --- | --- |
| `sequence` / `Sequence` | map, filter, fold, append, concat, take, drop, zip, unfold; finite lists | none |
| `array` / `Array` | persistent list-backed arrays; init, make, get, set, slice, map, fold; checked operations return Option, indexing is O(n) | none |
| `stream` / `Stream` | lazy, non-memoized streams; unfold, next, map, filter, take, fold | none |
| `getopt` / `Getopt` | long options, flags, `--name=value`, `--name value`, `--`; ordered options and positional arguments | none |
| `db` / `Db` | immutable key/value database; get, put, remove, keys, values, size, right-biased merge | none |
| `nosqldb` / `NoSQL` | immutable document collections; insert (reject duplicates), upsert, get, remove, find, update | none |
| `transpile` / `Transpile` | Result-returning source transformation pipelines, composition, replacement, indentation, joining | none |
| `weblets` / `Weblets` | escaped text/attribute values, elements, void elements and HTML documents | none |
| `http` / `Http` | HTTP/HTTPS requests, response status/headers/body, availability | `install_HTTP` (libcurl at runtime) |
| `uri` / `Uri` | parse/render, percent encoding, query parameters | `install_URI` |
| `ffi` / `Ffi` | shared-library symbol lookup and typed C calls | `install_FFI` |
| `memory` / `Memory` | mutable byte buffers, append, checked byte access/slicing, truncate, clear | `install_Memory` or `install_Buffer` |
| `shell` / `Shell` | POSIX quoting, argv-based execution, shell commands, environment, working directory | `install_Shell` or `install_Process` |
| `parsec` / `Parser` | parser combinators, regexes, alternatives, repetition, mapping/binding, complete/prefix parsing | `install_Parsec` |
| `jsonrpc` / `JsonRPC` | JSON-RPC 2.0 envelopes, dispatch, batches, notifications, parse/invalid-request errors | `install_JSONRPC` or `install_JSON` |
| `msgpack` / `MsgPack` | binary pack/unpack with exact signed integers, floats, strings, binary, arrays, maps and extensions | `install_MsgPack` |
| `sqlite` / `SQLite` | connect, parameterized query/execute, close | `install_SQLite` |
| `sqldb` / `SQL` | SQLite operations plus Result-based transactions with rollback | `install_SQLite` |
| `nng` / `Nng` | pair, req/rep, pub/sub sockets; listen, dial, subscribe, send, recv, close | `install_NNG` |
| `sysipc` / `SysIPC` | local POSIX sequenced-packet endpoint pairs; nonblocking send/recv, close | `install_IPC` |
| `webserver` / `Webserver` | background HTTP server, static response routes, text/HTML helpers, start/stop | `install_Webserver` |

### Host setup

The new C++ entry headers are `metalib/<directory>/<Name>.hpp`, e.g.
`metalib/sqlite/SQLite.hpp`. Installers are in namespace `metalib`. HTTP,
URI, FFI, Parsec, Memory, Shell and JSONRPC also expose D adapters under the
same directories, with camel-case installers (`installHTTP`, `installMemory`,
etc.). SQLite, NNG, MessagePack, IPC and Webserver currently have C++ backends.
Other native modules need only the existing portable runtime in either dialect.

```cpp
#include "metalib/sqlite/SQLite.hpp"

metaml::Session session;
metalib::install_SQLite(session);
session.allow_all_primitives(); // or select individual primitive capabilities
session.load_stdlib("metalib");
session.eval("import \"sqldb\"");
```

```ml
import "sqldb"
val connection := SQL.connect ":memory:"
val rows := match connection with
  | Err error -> Err error
  | Ok db -> SQL.query db "SELECT ? AS answer" [SqlInt 42]
```

SQLite query rows are lists of `(column_name, SqlValue)` pairs. `SqlValue`
preserves NULL, Int, Float, text and binary (`SqlNull`, `SqlInt`, `SqlFloat`,
`SqlText`, `SqlBlob`). Each query accepts exactly one statement with bound
parameters. Explicit close is idempotent; garbage collection also releases
handles. `SQL.transaction db callback` commits on `Ok` and rolls back on
`Err`; callbacks must return Result and retain the connection until completion.
Thrown interpreter errors are not Result values and must be handled by the host.

MessagePack uses `MsgValue`: `MpNil`, `MpBool`, `MpInt`, `MpFloat`, `MpString`,
`MpBinary`, `MpArray`, `MpMap`, `MpExt`. `pack` and `unpack` return Result.
Decoding rejects truncation, trailing bytes, reserved tags, nesting beyond 128,
and unsigned integers above Int's signed 64-bit range. Strings are byte strings;
UTF-8 validation is left to applications. Extension types range from -128 to 127.

NNG endpoints accept `inproc://`, `ipc://` and `tcp://` URLs supported by NNG.
Sockets use five-second send/receive timeouts; subscriptions are byte prefixes.
The backend targets the installed NNG 1.x API and must use matching headers and
library. IPC pairs preserve messages up to 65536 bytes, return Err when a
nonblocking operation cannot proceed, and may be inherited across `fork`.
An empty IPC packet and peer EOF both yield `Ok ""`.

Webserver route arguments are server, HTTP method, path pattern, status,
content type and body. Routes must be registered before start; cpp-httplib
matches path patterns. Start takes a host and port (0 selects a free port),
returning the bound port. Responses are captured immutable data, so worker
threads never enter the MetaML interpreter. Stop joins workers; destruction
also stops the server. Weblets tag/attribute names and rendered children must
be trusted; `Weblets.text` escapes user text.

JSON-RPC dispatch takes `method -> params -> Result Json (Int, String)`.
Applications implement method lookup and return `Err (~32601, "Method not found")`
for unknown methods. `handle` consumes JSON text and returns an optional JSON
response; notifications produce None. Transport is independent of dispatch.

### Dependencies and extension tests

```sh
cmake -S . -B build -DMETAML_BUILD_SERVICES=ON \
  -DMETAML_THIRD_PARTY_DIR=/mnt/warble/metatk/third_party
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

`METAML_BUILD_SERVICES` enables the `metaml-services` interface target and
service integration tests. It requires installed SQLite3 and NNG 1.x plus
`httplib.h`, found in the third-party `cpp-httplib` checkout or system includes.
If the configured third-party root is absent, the sibling `../third_party`
directory is used. No dependency is downloaded or copied into this repository.
The local MessagePack checkout may contain only metadata; its wire codec is
self-contained. Services are opt-in; portable extension tests always run.
`metalib_extensions` covers the new APIs, including SQLite rollback, NNG/IPC
round trips and a loopback HTTP request when services are enabled.
`metalib_extensions_d` verifies native modules and D adapters when a D compiler
is available.

## Host bundles

Include `metalib/cpp/Metalib.h` or import `metalib.d.Metalib`. Install host
bundles before loading the language library:

| C++ / D snake-case API | Contents | Dependencies |
| --- | --- | --- |
| `install_foundation` | String, List, Hash, Bridge, Buffer, Table, Graph | Core runtime |
| `install_data` | URI, JSON, TOML, YAML, SExpr | Foundation |
| `install_language` | AST builders/walkers, lexer/parser bridges, Frontend, Type, Module | Data |
| `install_io` | IO, Loader | Foundation |
| `install_all` | All above, plus Metalib.version | All layers |

D also provides `installFoundation`, `installData`, `installLanguage`,
`installIOBundle`, and `installAll`. Dependencies are installed automatically.
Bundle composition skips modules whose entry-point primitive is already
registered, preserving opaque types and existing handles. Install complete
modules rather than registering just their entry points manually.

Installation does not grant primitive capabilities; embedders select them with
the session allowlist. The CLI enables the portable host bundle. Platform/native
modules such as File, Process, CBridge and FFI have explicit individual installers.
The C embedding adapter is in `c/`; C++ `.c` files are header implementation
fragments, not standalone C translation units.

`IO.read_all` preserves input bytes, including CRLF, NUL and a missing final
newline. `IO.read_lines` splits lines and strips their line endings.

## Verification

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

When a D compiler is present, this includes mirrored runtime tests, CLI stream
contracts and cross-dialect fixture/AST/primitive-manifest comparisons.
