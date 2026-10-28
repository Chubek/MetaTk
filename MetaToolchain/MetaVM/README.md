# MetaVM status

`vmspec/` and `vmweave/` currently contain no source files. There is no VM,
bytecode interpreter, public API, build target or test suite here to refine.

Before implementing either subsystem, specify:

- the bytecode/schema and its validation/versioning rules;
- integer/trap, call, memory and host-interface semantics;
- frame layout and ownership, including resource limits;
- whether/how the VM records traces and transitions to MetaJIT code;
- executable examples and independent reference results for conformance tests.

Do not treat the directory names or MetaJIT's empty tracing/OSR/deoptimization
headers as implemented functionality. See `../META_REFINEMENT.md` for the current
cross-project audit and verification results.
