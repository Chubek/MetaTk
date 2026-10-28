# 20. Compiler contexts, results, and ownership

The embedding API is C++20. Include `<juniper/Juniper.hpp>` and link
`Juniper::Juniper`; the exported CMake target carries the required MetaIL linkage
and include paths.

## Understand the two error channels

`Compiler` construction can throw `std::runtime_error` when the real MetaIL
runtime cannot be initialized. Ordinary parse/check/transform operations return
`juniper::Result<T>`. Allocation and filesystem exceptions are not translated
into diagnostics. A moved-from compiler reports an invalid-context diagnostic
when used for the checked operations.

This complete program handles both construction exceptions and semantic results:

<!-- check: cpp -->
```cpp
#include <juniper/Juniper.hpp>
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        juniper::Compiler compiler(argc > 1 ? argv[1] : nullptr);
        auto parsed = compiler.parse(
            "fn increment(x) { return add(x,1); }", "inline.junir");
        if (!parsed) {
            std::cerr << juniper::format(*parsed.error()) << '\n';
            return 1;
        }
        auto emitted = compiler.emit(*parsed.get());
        if (!emitted) {
            std::cerr << juniper::format(*emitted.error()) << '\n';
            return 1;
        }
        std::cout << *emitted.get();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

`get()` returns a pointer to the value **inside the Result**; it is not a new
allocation. Test the result before dereferencing that pointer. On failure,
`error()` points to the contained diagnostic. Neither pointer outlives its result
object or survives arbitrary moves of that object.

## Owned IR does not borrow its context

`Module`, `Function`, `Statement`, and `Expr` own strings and vectors. A parsed
module can outlive the compiler that produced it. Copying it makes an independent
value tree. Optimization returns a new module and does not modify its input.
Strings from `emit`, `emit_metail`, and `analyze` are also owned values; do not call
`metail_free` on any of these C++ objects.

The compiler itself owns a runtime and is move-only. Serialize calls on one
context. Independent contexts can be used by independent threads; do not mutate
one module concurrently with another thread reading it.

## A small application helper for later examples

The supplied [Support.hpp](examples/Support.hpp) defines these application
helpers, not new Juniper APIs:

```cpp
namespace example {
template<class T>
T take(juniper::Result<T> result) {
    if (!result)
        throw std::runtime_error(juniper::format(*result.error()));
    return std::move(*result.get());
}

inline void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}
```

Later standalone examples use `example::take` inside a `try`/`catch` to keep the
interesting logic visible. This is an explicit application decision to turn a
failed Result into an exception, not an exception mode enabled inside Juniper.
You can instead keep the manual `if (!result)` style throughout your host.

`Compiler::parse` expects already-expanded source. Use `load(path, options)` for
macro files; passing `@fn(...)` directly to `parse` is not a preprocessing request.
For free-function `select`, pass the same explicit runtime root you used to
construct the compiler when relying on a nonstandard deployment.

[Previous](19-analysis.md) · [Index](README.md) · [Next](21-native-ir.md)
