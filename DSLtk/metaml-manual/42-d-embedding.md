# 42. Embedding in D

The D dialect mirrors the C++ language surface while following D allocation and delegate conventions. Import `MetaML` and construct a Session with `new`. Both snake_case methods and several camelCase aliases are available; this manual uses snake_case where it matches the shared API.

```d
import MetaML;
import std.stdio : writeln, stderr;
int main(string[] args) {
    auto session = new Session;
    auto loaded = session.load_stdlib(args.length > 1 ? args[1] : "metalib");
    if (loaded.is_err()) return 1;
    auto result = session.eval("List.sum [10, 20, 12]");
    if (result.is_err()) {
        foreach (d; result.unwrap_err()) stderr.write(session.render(d));
        return 1;
    }
    writeln(to_string(result.unwrap()));
    return 0;
}
```

Typed registration supports D numeric types, bool, string, Nullable, arrays, tuples, and Value. A registration such as `session.register_primitive("Host.inc", (long n) => n + 1);` exposes an Int function. A low-level PrimFn receives a PrimContext and Value array, enabling callbacks and structured values.

To install host libraries, import `metalib.d.Metalib` and call `installAll(session)`, then configure the same primitive allowlist used by C++ clients. When building with LDC, `-i` compiles imported source modules automatically. Alternatively, include the nonempty host modules explicitly in the build, as the CMake tests do.

Recoverable D Exceptions raised by invoked host primitives become `M0409`; language MetaError diagnostics retain their original codes. Defects and process-level faults are not ordinary recoverable user input. Garbage collection simplifies object retention but does not eliminate cycles, resource ownership, or external-handle cleanup concerns.

Parity means the same MetaML source has the same observable behavior, not that the native object layouts or memory-management strategies match. Keep dialect-specific conveniences below the language boundary, and test new primitive names, type signatures, results, errors, and AST dumps against both implementations.

<!-- nav:begin -->
[Contents](README.md) · [Previous](41-cpp-embedding.md) · [Next](43-c-embedding.md)
<!-- nav:end -->
