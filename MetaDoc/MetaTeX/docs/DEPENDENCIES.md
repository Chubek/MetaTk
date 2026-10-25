# Third-party dependency status

Dependencies are resolved from the enclosing metatk workspace. CMake never
downloads dependencies. See `cmake/Dependencies.cmake` for configurable paths.

## Actual build dependencies

- Required: C++20 standard library, CMake, POSIX process APIs and threads.
- `third_party/dynalo`: shared-library loading and typed symbol lookup.
  Upstream: https://github.com/maddouri/dynalo, MIT license, vendored revision
  `411199d712003195966b380bb4f5dcd2d63f4733`. The missing checkout was populated
  for this integration; its upstream `LICENSE` is retained.
- `third_party/replxx`: interactive editing, completion and history (BSD-3-Clause).
- `DSLtk/MetaML.hpp` and `DSLtk/metalib`: MetaML runtime and all implemented
  host/language libraries. If a workspace-root `MetaML.hpp` exists it is preferred;
  `METATEX_METAML_DIR` explicitly selects the matching header/library tree.
- SQLite3 and NNG development libraries, plus
  `third_party/cpp-httplib/httplib.h`, for MetaML's service modules.
  MetaML HTTP loads libcurl at runtime; native FFI loads the requested libraries.
- Sibling `MetaVect/scene`: retained scene interchange.
- Optional: `pangocairo` development package; it uses Cairo, Pango,
  HarfBuzz, FreeType and Fontconfig to render PDF with Unicode shaping.
- Testing: CTest (bundled with CMake); Python 3 for extended tests if found.

Path overrides: `METATEX_THIRD_PARTY_DIR`, `METATEX_DYNALO_DIR`,
`METATEX_REPLXX_DIR`, `METATEX_METAML_DIR`, and `METATEX_METAVECT_SOURCE_DIR`.

## Library integration roadmap

- **Typography / output**: cairo, pango, fontconfig, freetype, harfbuzz,
  Grafitt, libharu, PDF-Writer, OpenImageIO, jpeginfo, libpnmio, nanosvg,
  stb, libdrm — only the optional PangoCairo integration is implemented.
- **Text / scanning / IR**: libunistring, utf8proc, md4c, libchomsky3,
  texscrape, SerdeTk, Klyspec, Polyflow, ctl, xxHash, xsimd, simdette —
  use of these is not yet implemented; current core uses C++ standard library.
- **Scripting / foreign language**: micropython, mruby, S7, QaMRpp,
  nanobind, jiterati, qbe — not integrated.
- **System / networking / diagnostics**: cpp-httplib and NNG are integrated
  through MetaML, and replxx supplies the REPL. bintoolchest, jsonrpc-c,
  libdwarfpp, LIEF, memtkx, memtkx-c, jemalloc and PikoRL are not integrated.
- **Serialization / testing**: msgpack-c, tomlplusplus, Catch2 — not
  integrated; JSON is generated without dependencies and CTest is used.

Names in the request can vary by upstream capitalization and spelling (for
example `Grafitt` vs Graphite2; `NNG` vs `nng`). Dependencies should be
introduced after verifying their exact repository, API, license and ABI.
Do not silently claim a library has been integrated merely because it appears
in a dependency manifest.
