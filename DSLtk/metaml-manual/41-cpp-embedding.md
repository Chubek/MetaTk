# 41. Embedding in C++

The core C++ interpreter is header-only. A minimal application includes `MetaML.hpp`, constructs a Session, loads the portable library explicitly, evaluates source, and checks the returned Result. The runtime's ordinary API returns diagnostics rather than throwing its internal language-error object across the boundary.

```cpp
#include "MetaML.hpp"
#include <iostream>
int main(int argc, char **argv) {
    metaml::Session session;
    auto loaded = session.load_stdlib(argc > 1 ? argv[1] : "metalib");
    if (loaded.is_err()) return 1;
    auto result = session.eval("List.sum [10, 20, 12]");
    if (result.is_err()) {
        for (const auto &d : result.unwrap_err()) std::cerr << session.render(d);
        return 1;
    }
    std::cout << metaml::to_string(result.unwrap()) << '\n';
}
```

Typed primitive registration derives a MetaML type from supported C++ signatures: integral/floating types, bool, strings, optional, vectors, pairs, and selected Result types. For example, `session.register_primitive("Host.inc", [](std::int64_t x) { return x + 1; });` explicitly trusts that primitive and grants its exact name. Use the low-level typed-source/arity/PrimFn overload when calling back into MetaML or working with opaque handles.

Sandboxed libraries use `install_primitive` instead. `metalib::install_all(session)` installs the portable host bundle; subsequently set a narrow allowlist such as `{"Hash.*", "Buffer.*"}`. Install before loading the standard library when host declarations add types and constructors.

`global` retrieves a bound value; `call` applies a function Value. Keep the Session alive while using its values: closures and primitive pointers are associated with session-owned machinery. Re-registering a primitive preserves old callable objects' lifetime, but does not make them independent of the Session.

A Session is not generally safe for concurrent evaluation. Serialize access, and do not mistake handle-local mutexes for a global concurrency guarantee. Recoverable standard exceptions from invoked host primitives become `M0409`; native faults, undefined behavior, and process termination cannot be recovered this way.

<!-- nav:begin -->
[Contents](README.md) · [Previous](40-language-tooling.md) · [Next](42-d-embedding.md)
<!-- nav:end -->
