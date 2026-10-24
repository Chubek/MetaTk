# 44. Permissions, testing, and deployment

The primitive allowlist is a capability filter for non-core registered primitives. It accepts exact names, `Module.*` patterns, or a global wildcard. `register_primitive` is explicitly trusted; library `install_primitive` requires later permission. The supplied REPL permits all installed primitives because it is an interactive developer tool, not a hardened service boundary.

Core IO, source imports, eager effects, native allocations, and unbounded tail-recursive execution remain relevant even with an empty allowlist. Use operating-system process isolation, timeouts, memory limits, filesystem restrictions, and a deliberately reduced runtime when handling hostile programs. Never expose native FFI or process execution merely because a parser accepted the source.

The portable bundle excludes the C++-only POSIX File/Process and native FFI/CBridge modules. Those headers are separate opt-ins and currently lack implemented D twins. HTTP, Webserver, Fiber, Channels, and Threading files are placeholders, not shipped functionality. Keep unsupported modules out of parity claims and deployment promises.

```sh
python3 scripts/run_tests.py --cpp build/metaml-repl-cpp --d build/metaml-repl-d
python3 scripts/parity_check.py --cpp build/metaml-repl-cpp --d build/metaml-repl-d --trace
python3 scripts/check_manual.py --cpp build/metaml-repl-cpp --d build/metaml-repl-d
```

Tests compare successful computations, expected errors, stdout/stderr, and canonical ASTs. Native regressions exercise callbacks, malformed indices, allocation bounds, import retry, primitive replacement, and C ownership rules. Add a fixture before fixing a bug, and test a later successful operation after failure to verify recovery.

For deployment, ship a known standard-library tree and select it explicitly rather than depending on the working directory. Keep interpreter and library versions together. Installed headers must retain their relative directory layout; D module imports likewise depend on the module tree. Run smoke tests against an installed prefix, not only a source checkout.

No statement in this manual implies exhaustive parser-standard conformance, constant-space behavior, complete security isolation, or full implementation of every reserved library file.

<!-- nav:begin -->
[Contents](README.md) · [Previous](43-c-embedding.md) · [Next](45-reference.md)
<!-- nav:end -->
