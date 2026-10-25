# Chapter 28 — Runtime and Internals

**Status:** scaffold contracts (`internals/*.d`, ~45 modules). None execute
today; the frontend performs no runtime services.

## Subsystem survey

- **Values and memory:** `Value`, `NaNBoxing`, `Memory`, `GC`,
  `StringPool`, `SIMD` — representation and allocation strategy.
- **Execution:** `Runtime`, `EventLoop`, `Async`, `Fiber`, `Channel`,
  `Computation`, `Math` — evaluation and concurrency primitives.
- **Programs:** `Module`, `Loader`, `Linker`, `Package`, `Environ`,
  `ProgramImage`, `Prelude`, `Builtins` — units, linking, and startup.
- **I/O and interop:** `Reader`, `Writer`, `FFI`, `Interop`, `IPC`,
  `Shell`, `Loader` — explicit boundaries; FFI and filesystem operations
  stay explicit per the working agreements, never ambient.
- **Tooling:** `Diagnostics`, `Logger`, `Profiler`, `REPL`, `Printer`,
  `Formatter`, `Cache`, `ASTSerializer`, `MVIRSerializer`, `Renderer`,
  `Shader`, `FrameBuffer`, `Types`, `BackendTraits`.

## Standing rule

If a subsystem remains a stub, its module docs state the expected
contract; no unsupported runtime behavior is fabricated. Runtime work
begins only after MVIR and selection (chapters 22–25) can hand it
well-formed terms.
