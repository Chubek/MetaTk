# C++ API and native DSL

Include `MetaTeX.hpp` and link `metatex_core` with a C++20 compiler. This header
exposes the existing `Engine`, `Document`, `Block`, `Span`, lexer, math and
renderer APIs, plus an owning document builder and DSLtk pipeline stages.

## Build integration

In a CMake project that includes the MetaTeX source tree:

```cmake
add_subdirectory(path/to/MetaTeX metatex-build)
add_executable(report main.cpp)
target_link_libraries(report PRIVATE metatex_core)
```

The target propagates C++20 and the required include paths. DSLtk comes from
`METATEX_DSLTK_DIR`, defaulting to the workspace's `DSLtk` directory. Override
this cache variable when configuring a standalone checkout. No download is
performed. Installation places `DSLtk.hpp` beside `MetaTeX.hpp` and the other
public headers, with libraries under `lib/metatex`.

The extension is header-only, but compilation and rendering require the linked
MetaTeX core library. Existing applications may continue including `MetaTeX.h`.

## Programmatic document construction

```cpp
#include "MetaTeX.hpp"

using namespace metatex::cpp;

metatex::DocumentBuilder builder;
builder.title("Report").author("Application").date("2026-10-25");
builder.append(heading(1, "Results") | withId("results"));
for (const auto& name : {"Alpha", "Beta"})
    builder.append(paragraph("Result: ", bold(name)));
builder.reference("results", "1");
builder.append(paragraph("See section ", ref("results")));

metatex::Document doc = builder.build();
metatex::writeDocument(doc, "report.html", metatex::OutputFormat::HTML);
```

`build()` on an lvalue returns an independent snapshot; `std::move(builder).build()`
transfers its document. Construct a builder from a `Document` to extend an engine
compilation or an existing native document. `diagnostic(Diagnostic)` adds a
diagnostic; `hasErrors(doc)` checks for error-severity diagnostics.

Factories return ordinary IR values. You can store them in containers, inspect
or edit their fields, and pass them to `DocumentBuilder::append`. For dynamic
span lists, construct a `Block` and fill its `spans` vector directly.

## Declarative C++ DSL

```cpp
using namespace metatex::cpp;
using namespace metatex::cpp::literals;

auto doc = document(
    heading(0, "Native report"),
    tableOfContents(),
    heading(1, "Results") | withId("results"),
    paragraph("Hello "_text, bold("world"), ". Formula: ", "x^2"_math),
    listItem(1, false, "First result"),
    note("Computed by the application"),
    displayMath(R"(\frac{1}{2})")
) | title("Native report") | author("Application");

auto html = doc | renderAs(metatex::OutputFormat::HTML);
auto json = doc | renderAs(metatex::OutputFormat::JSON);
doc | writeTo("report.pdf", metatex::OutputFormat::PDF);
```

`document(...)` accepts blocks; content factories accept any mix of `Span` values
and strings convertible to `std::string_view`. Strings are copied immediately.
Text is literal: TeX commands, braces, dollar signs, and whitespace are preserved
in the IR. Output escaping is handled by the existing renderers. `math`,
`displayMath`, and `_math` accept the engine's existing TeX-math subset.

### Vocabulary

| Kind | Factories |
| --- | --- |
| Inline | `text`, `bold`, `italic`, `code`, `smallCaps`, `underline`, `math` |
| Links/media | `link(label, target)`, `image(alt, source)`, `ref(key)` |
| Blocks | `paragraph(...)`, `heading(level, ...)`, `listItem(level, ordered, ...)`, `quote(...)`, `note(...)` |
| Other blocks | `codeBlock(text)`, `displayMath(tex)`, `horizontalRule()`, `tableOfContents()`, `pageBreak()` |
| Vector scene | `graphic(metavect::Scene, caption, width)` |
| Low-level block | `block(BlockKind, ...)` |
| Block stage | `withId(id)` |
| Document stages | `title(value)`, `author(value)`, `date(value)`, `reference(key, displayValue)`, `append(block)` |
| Processing stages | `compileWith(engine, filename)`, `renderAs(format)`, `writeTo(path, format)` |
| Literals (opt-in) | `"text"_text`, `"x^2"_math` in `metatex::cpp::literals` |

Heading levels follow the existing IR: 0 is a document title, 1 a section,
through 5. List indentation starts at 1. Invalid levels throw
`std::invalid_argument`. Headings do not auto-number or allocate IDs; use
`withId` for anchors and `reference` for reference display values. Metadata does
not create title blocks: use `heading(0, ...)` when a visible title is needed.
Vector width is 0 for natural width, or 0.01–1,000,000 points; invalid or
non-finite widths throw `std::invalid_argument`.

Stages use DSLtk's `dsl::pipe` and compose with application stages:

```cpp
auto count = doc | dsl::pipe([](const metatex::Document& d) {
    return d.blocks.size();
});
```

Document-transforming stages operate on values: lvalue inputs are copied and
rvalues are moved. Stages own their arguments and are reusable. `writeTo` writes
the output and returns the document for subsequent stages. `compileWith` borrows
its engine, which must outlive the stage.

## TeX compilation, commands and plugins

```cpp
metatex::Engine engine;
engine.registerCommand("app", [](metatex::Engine& e) {
    e.addSpan(metatex::SpanKind::Bold, e.readTextArgument());
});

auto doc = R"(Hello \app{application})" | compileWith(engine, "report.tex");
if (!metatex::hasErrors(doc))
    metatex::writeDocument(doc, "report.html", metatex::OutputFormat::HTML);
```

Use `metatex::compile(source, filename)` for a one-shot plain-engine compilation,
or `compileFile(engine, path)` / `compileFile(path)` to load a file. Relative
`\input` resolution and diagnostics use the supplied filename. The original
`Engine::compileFragment` supports incremental evaluation; `Engine::compile`
and `compileWith` reset document state as usual. Registered command handlers,
environments, macros, counters, and the `metatex_register_plugin(Engine*)` entry
point remain available through the existing engine API. Dialect plugins must
be registered on that engine by the application before compiling dialect input.

Compilation returns diagnostics in the document; rendering does not reject a
document containing errors. I/O failures and engine resource-limit exceptions
propagate to the caller.

## Rendering

`render(doc, OutputFormat)` returns bytes for HTML, JSON, text, or the basic
in-memory PDF writer. `writeDocument` writes those bytes to a file. The existing
`renderHTML`, `renderJSON`, `renderText`, `renderPDF`, and optional
`renderCairoPDF(doc, path)` functions also accept native documents directly.
Backend capabilities and the supported TeX subset are unchanged.

See [`examples/native.cpp`](../examples/native.cpp) for a compiled example and
`cpp-api-dsl` / `cpp-native-example` in CTest for executable coverage.
