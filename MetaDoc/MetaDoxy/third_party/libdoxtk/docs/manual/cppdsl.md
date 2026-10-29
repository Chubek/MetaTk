# Optional C++ native DSL

The C++ builder uses DSLtk's CRTP `DSL`/`Pipeline` feature and pipe stages from `third_party/metatk/DSLtk/DSLtk.hpp`. It builds the existing partial MDAST model, not a new document IR, Lua Doxir runtime, or Markdown serializer.

## Enable and build

```sh
cmake -S . -B build-cppdsl -DDOXTK_ENABLE_CPP_DSL=ON \
  -DDOXTK_DSLTK_ROOT="$PWD/third_party/metatk/DSLtk"
cmake --build build-cppdsl
ctest --test-dir build-cppdsl --output-on-failure
build-cppdsl/doxtk_cppdsl_example
```

Without an explicit `DOXTK_DSLTK_ROOT`, configure first looks for a system DSLtk CMake package/header. An explicit directory is authoritative: a missing header fails rather than silently selecting another copy. Configure checks that the selected header's DSL/Pipeline API compiles with the active C++20 toolchain. Nothing is downloaded, and the complete metatk build is not configured. DSLtk's optional Lua integration is not enabled by DoxTk.

Link `DoxTk::CPPDSL` to enable the builder; it links the core library transitively and supplies `DOXTK_ENABLE_CPP_DSL=1` and the dependency include directory. Linking only `DoxTk::DoxTk` does **not** enable it, even in a DSL-enabled build. `<DoxTk.hpp>` then exposes `doxtk::cppdsl::DocumentDSL`; the focused `<doxtk/DoxTk-CPPDSL.hpp>` is also self-contained. With the option off, these headers never include DSLtk and the builder is absent.

## Compose trees

```cpp
#include <DoxTk.hpp>

doxtk::cppdsl::DocumentDSL d;
auto result = d.wrap(d.document({
    d.heading(1, {d.text("Title")}),
    d.paragraph({d.text("Hello "), d.strong({d.text("world")}), d.text("!")}),
    d.list({d.item({d.paragraph({d.text("A task")})}, true)})
})) | d.finish({{"author", "Example"}});

if (result) {
    auto dump = doxtk::builtin_conversions().translate(*result.value, "DoxTkJSON");
    // Inspect dump.value->data or dump.error.
} else {
    // Inspect result.error.code/message/context.
}
```

Factories return `Result<Node>`, and `Children` is a vector of those results. Paragraphs, headings, emphasis, strong, and links accept phrase children. Documents, blockquotes, and list items accept blocks. Lists accept only list items. Headings require depth 1..6. Available leaves are `text`, `inline_code`, `line_break`, `thematic_break`, `code(value, language)`, `image(alt, url, title)`, and `verbatim(value, reason)`. `link(url, children, title)` and `list(items, ordered, start)` accept optional attributes; start is used only for ordered lists. `item(children, checked)` takes an optional checkbox state.

`build(root, metadata)` is the non-pipeline equivalent of `wrap(root) | finish(metadata)`. Nested expected errors propagate as structured results; no nodes are replaced with placeholders. UTF-8 text/attributes and metadata are validated, attributes are sorted, and the core AST nesting limit applies. Empty documents are valid. Embedded NUL is retained: JSON dumps support it, XML dumps reject it. Diagnostics supplied on child results survive successful composition.

The resulting `Document` holds an immutable tree in `tree`, `ir="MDAST"`, and `media_type="text/markdown"`. Its `data` is empty: a builder has no original Markdown source. Synthesized nodes have unavailable source positions, represented as null/invalid in dumps. Normal AST dumps and the explicit lossy Plaintext route work unchanged; metadata is never silently discarded. The builder does not infer Markdown source, sanitize link destinations for browser use, or implement full MDAST/unist schema compatibility. `build()` also accepts externally constructed roots, which receive core `Node::validate()` checks; factory child restrictions are not a substitute for a full schema validator.

## Installed consumers

When enabled, installation exports `DoxTk::CPPDSL` and includes the selected standalone DSLtk header with its embedded license notices under `include/doxtk/dsltk`. The exported include paths are relative to the installation prefix; no source-tree paths are needed. Core-only installations do not include that header or export the interface target.

```cmake
find_package(DoxTk CONFIG REQUIRED COMPONENTS CPPDSL)
target_link_libraries(my_app PRIVATE DoxTk::CPPDSL)
```

Smoke-test the installed component with:

```sh
cmake --install build-cppdsl --prefix "$PWD/build-cppdsl/install"
cmake -S tests/package -B build-cppdsl/consumer \
  -DCMAKE_PREFIX_PATH="$PWD/build-cppdsl/install" -DDOXTK_CONSUMER_CPPDSL=ON
cmake --build build-cppdsl/consumer
build-cppdsl/consumer/consumer
```
