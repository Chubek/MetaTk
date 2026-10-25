# 38. Testing and debugging

The standard verification sequence is:

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
build/metail verify
python3 tools/generate_kernels.py --check
```

Tests cover the CLI, native numeric binding, loader rollback, C text APIs, C
structured descriptors, C++ registry and DSL operations, and compiled examples.
The parser-free regression disables `parser.program` while exercising the native
IR path. Native tests also cover ownership after context destruction, unchanged
inputs, malformed descriptors, cycles, invalid arities, effects, traps, and overflow.

The rewrite verifier checks every declarative rule fires and compares semantic
outcomes across signed-64 boundary and deterministic test vectors, including call
traces and trap cases. Passing it is stronger than merely observing a pretty
printed simplification, but it is not a formal proof of arbitrary future rules.

For sanitizers, configure a separate Debug build with
`-fsanitize=address,undefined -fno-omit-frame-pointer` in both C and C++ flags.
Use GCC and Clang independently and enable `-Wall -Wextra -Wpedantic -Werror`.

When debugging a native expression, inspect `expr.ast().tag()`, its children, or
`expr.dump()`. When debugging a pass, restrict the pipeline to that pass and inspect
both structural output and statistics. Include source names on textual calls so
parser errors identify the relevant input fixture.

[Contents](README.md) · [Next: Maintenance](39-maintenance.md)
