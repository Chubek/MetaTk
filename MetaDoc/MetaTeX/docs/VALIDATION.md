# Integration validation

The CTest suite includes core regressions, document/plugin output, native vector
geometry, scripting primitives, all MetaML extension imports, SQLite queries,
shebang execution, descriptor selection, process failures/timeouts/output limits,
REPL golden transcripts, and a real PTY check for replxx completion/history.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

## Sanitizer finding in the MetaML dependency

The full AddressSanitizer/UndefinedBehaviorSanitizer run with the built-in PDF
backend reports **LeakSanitizer failures in MetaML session teardown**. A single
simple command importing the prelude and installed host modules retained about
1.8 MiB in 12,393 allocations in the observed run. Stack traces originate in
`DSLtk/MetaML.hpp` (`Scope`, type/constructor and module graphs created during
`Session::load_stdlib` and installer declarations). Calling `Session::close()`
and destroying the session does not collect these shared-ownership cycles.

Repeated MetaML invocations in a long-lived REPL can therefore accumulate memory.
This dependency issue is unresolved here. No suppression is built into MetaTeX
or its tests, and a leak-enabled sanitizer run must not be reported as clean.
An additional run with `ASAN_OPTIONS=detect_leaks=0` checks invalid memory accesses
and undefined behavior separately; that run does not verify leak freedom.
