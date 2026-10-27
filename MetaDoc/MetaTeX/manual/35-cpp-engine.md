# 35 — Embedding the C++ engine

Include `MetaTeX.h` and link the CMake target `metatex_core`. An engine can be
used directly without the CLI:

```cpp
#include "MetaTeX.h"

metatex::Engine engine;
auto document = engine.compile("Hello, \\textbf{library}!", "report.tex");
auto html = metatex::renderHTML(document);
auto json = metatex::renderJSON(document);
```

Before publishing output, inspect `document.diagnostics` for entries whose
severity is `Diagnostic::Severity::Error`. Compilation can also throw, for
example when source encoding or a resource budget is invalid. The library
does not apply the CLI's output-on-error policy automatically.

`compile` resets document-defined macros, registers, metadata, and other
batch state. Externally registered handlers remain available. `compileFragment`
preserves session definitions and returns the newly generated blocks while
retaining metadata and references. Use separate engines for independent
concurrent work; no thread-safety guarantee is made for a shared mutable engine.

The filename argument supplies diagnostic context and the base path for
relative inputs. The result is a value document, with shared immutable storage
for vector scenes, and can be rendered after later compilations. No backend
should call back into source parsing. Convenience wrappers such as `compileFile`
and `hasErrors` are available in `MetaTeX.hpp`.

[Contents](README.md)
