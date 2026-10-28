# 23. Embedding GPPX preprocessing

`preprocess(path, options)` runs the real GPPX executable in a separate bounded
process. This isolates the process-global preprocessor engine; it is not an
in-process replacement library or a shell command string.

## Expand, inspect dependencies, then parse

The following complete example takes the MetaIL runtime root as its first
argument and the Juniper source root as its second argument:

<!-- check: cpp -->
```cpp
#include "Support.hpp"
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: example METAIL_HOME JUNIPER_SOURCE_ROOT\n";
        return 2;
    }
    try {
        const auto root = std::filesystem::path(argv[2]);
        const auto file = root / "manual/examples/includes/Juniperfile";
        juniper::PreprocessOptions options;
        options.timeout_seconds = 3;
        options.max_output_bytes = 1024 * 1024;
        options.defines["FEATURE"] = "1";
        options.include_directories.push_back(root / "manual/examples");

        auto expanded = example::take(juniper::preprocess(file, options));
        example::expect(expanded.dependencies.size() >= 4,
                        "expected root, prelude, and nested includes");
        for (const auto& dependency : expanded.dependencies)
            std::cout << dependency << '\n';

        juniper::Compiler compiler(argv[1]);
        auto parsed = example::take(compiler.parse(expanded.text, file.string()));
        auto loaded = example::take(compiler.load(file, options));
        example::expect(example::take(compiler.emit(parsed)) ==
                        example::take(compiler.emit(loaded)),
                        "load differs from preprocess followed by parse");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

`ExpandedSource` owns both its text and dependency paths. You may save it before
creating any compiler context. Preprocessing alone does not require a MetaIL
context or validate IR; the subsequent `parse` operation does.

## Control resources explicitly

`PreprocessOptions` exposes executable and standard-library paths, include
directories, macro definitions, output bytes, and timeout seconds. Empty
executable/library paths trigger the documented discovery rules. Names in
`defines` are bare identifiers; values cannot contain embedded newlines/NUL.

These are **fragments**, not mandatory settings for every host:

```cpp
juniper::PreprocessOptions options;
options.executable = "/opt/my-toolchain/bin/gppx";
options.standard_library = "/opt/my-toolchain/share/juniper/stdlib/Juniper.xpp";
options.defines["SCALE"] = "16";
options.timeout_seconds = 5;
options.max_output_bytes = 2 * 1024 * 1024;
```

Timeouts must be `1..3600`; the API allows output budgets through 64 MiB and up
to 120 explicit include directories. The later IR parser still has its separate
8 MiB source limit. Increasing a process output budget does not increase the
language's structural limits. GPPX stderr is bounded at 64 KiB; warnings are
reported as failures rather than discarded silently.

## Build systems and trust

Use `dependencies` to integrate file watching or caching, but also record the
configuration and selected runtime/tool versions. Includes in inactive branches
are not actual dependencies of that expansion. Do not confuse that fact with
a complete model of changes to include search precedence.

The driver does not invoke a shell and does not enable `#exec`. It still permits
file reads through includes. External executable paths and macro sources are
trusted inputs; time/output bounds are not a filesystem sandbox or a substitute
for process isolation when handling hostile input.

[Previous](22-editing-ir.md) · [Index](README.md) · [Next](24-backend-profiles.md)
