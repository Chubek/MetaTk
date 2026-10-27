# 36 — The C++ document DSL

`MetaTeX.hpp` layers a C++20, DSLtk-based vocabulary over the same public
document structures. Factories create ordinary owned blocks and spans:

```cpp
#include "MetaTeX.hpp"
using namespace metatex::cpp;

auto document = metatex::cpp::document(
    heading(1, "Results"),
    paragraph("Hello, ", bold("MetaTeX"), "."),
    paragraph("Formula: ", math("x^2"))
) | title("Native report");
auto html = document | renderAs(metatex::OutputFormat::HTML);
```

Factories include text, bold, italic, code, math, links, references, headings,
list items, quotes, notes, code blocks, display math, horizontal rules, page
breaks, and vector graphics. Heading levels and graphic widths are validated.
Pipelines can set metadata, add references, append blocks, render, or write.

`DocumentBuilder` offers imperative construction for dynamic content. Building
from an lvalue returns a snapshot; building from an rvalue transfers ownership.
The factories do not keep dangling views into temporary input strings.

`compileWith(engine)` borrows an existing engine for a pipeline; the engine
must outlive that pipeline's use. `renderAs(PDF)` uses the in-memory basic PDF
writer, while the Cairo/Pango function is separately available. A native math
span still uses the implemented math subset rather than providing a complete
mathematical layout tree. See [the API guide](../docs/CPP_API.md) for more
composition examples and the distinction between owned documents and borrowed
execution services.

[Contents](README.md)
