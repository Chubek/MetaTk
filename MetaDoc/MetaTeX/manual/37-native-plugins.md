# 37 — Writing native plugins

A plugin is a shared module linked to `metatex_core`. Export these entry
points and register handlers through the supplied engine:

```cpp
#include "MetaTeX.h"
extern "C" const char* metatex_plugin_id() { return "sample"; }
extern "C" void metatex_register_plugin(metatex::Engine* engine) {
    engine->registerCommand("hello", [](metatex::Engine& e) {
        e.addText("Hello from the plugin");
    });
}
```

```cmake
add_library(metatex_sample MODULE sample.cpp)
set_target_properties(metatex_sample PROPERTIES PREFIX "")
target_link_libraries(metatex_sample PRIVATE metatex_core)
```

Load it with `--plugin sample` after putting the module in a searched directory.
The identifier must match the requested name. C linkage fixes exported names,
not the C++ ABI of `Engine`, lambdas, standard-library types, or shared data.

Handlers read arguments with `readArgument`, `readOptionalArgument`, and text
helpers. Use `execute` for a token argument that should run now, `insert` for
tokens to process next, and `addText`/`addSpan`/`startBlock` to construct semantics.
Report malformed input with `report(message, true)`.

`registerCommand` is the host/plugin registration API. `defineCommand` creates
a group-local handler definition. Handlers are unexpandable; storing one in
an expanded macro definition does not execute it. Keep backend-specific work
out of handlers when it can be represented in `Document`, so all renderers
retain access to the feature's meaning.

[Contents](README.md)
