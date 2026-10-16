# 2. Architecture and collective C++ API

## Component graph

```text
custom source -> ParseTk -> events / DSLtk AST -> application lowering
                            |                         |
                            v                         v
                         Rewrite <---------------> MetaIL
                                                      |
                              +-----------------------+------------------+
                              |                       |                  |
                         text / MCIL                MetaVM       Linux x86-64 lowering
                                                      |                  |
                                                  Execution            ASMtk
                                                                         |
                                                                   ELF ET_REL
                                                                         |
                                                                        LDtk
                                                                         |
                                                                   ELF ET_EXEC
```

The application defines the semantics of its custom language. Parsing a grammar
does not automatically infer how a token maps to an IL operation. MetaChain
provides the substrates and adapters; it does not invent language semantics.

## Collective namespace

`MetaChain.hpp` includes every public header under `metachain/`. Its aliases
retain access to the underlying component APIs:

| Alias | Original namespace |
|---|---|
| `metachain::parsing` | `parsetk` |
| `metachain::rewriting` | `rewrite` |
| `metachain::il` | `metail` |
| `metachain::vm` | `metavm` |
| `metachain::jit` | `metajit` |
| `metachain::assembly` | `asmtk` |
| `metachain::linking` | `ld_tk` |

`metachain::version` is an inline `string_view`, mirrored by the
`METACHAIN_VERSION_MAJOR/MINOR/PATCH` macros for conditional compilation. It is
a library version, not a promise that every binary format uses the same
numbering; MCIL has its own wire-format version.

## Results and diagnostics

`Outcome<T>` contains `optional<T> value` and `vector<Diagnostic> diagnostics`.
Its boolean conversion succeeds only when a value exists and diagnostics are
empty. `*result` and `result->member` require a value. They use checked optional
access; they are not substitutes for testing the result.

Some failed operations retain useful partial results. For example, execution
can retain output before a trap, and rewriting can retain the last term before
a cycle or step limit. Therefore `value.has_value()` alone is not a success
test. A failed compilation or failed assembly does not yield a runnable
artifact through the facade.

`Diagnostic` contains:

* `stage`: `Parse`, `Verify`, `Optimize`, `Execute`, `Assemble`, `Link`, `Lower`,
  `Rewrite`, or `IO` (`IO` marks filesystem failures from `link_files`);
* `message` and optional `file`;
* `line`, `column`, and `offset`.

Text coordinates are one-based when present. Zero means unavailable. The
meaning of `offset` depends on the stage: a byte offset for input formats, an IL
instruction index for IL failures, or a rewrite count for normalization limits.
`format(diagnostic)` produces a compact human-readable message; inspect the
structured fields when building an IDE or protocol.

## Toolchain configuration

`Toolchain` owns an `Options` value. Construction copies/moves that configuration;
`options()` returns a const reference. Configure options before constructing a
toolchain. No mutable process-global configuration is used.

```cpp
metachain::Options options;
options.vm.max_steps = 100'000;
options.assembler.max_output_bytes = 4 * 1024 * 1024;
options.linker.image_base = 0x400000;
options.optimize = false;
metachain::Toolchain toolchain(options);
```

`Options` aggregates `il`, `vm`, `jit`, `assembler`, `linker`, `rewriting`,
`parser`, and `text` limit/options structures. Optimization defaults to enabled.

## Principal methods

| Method | Result |
|---|---|
| `compile(source, file = {})` | parsed, verified, optionally optimized IL |
| `decode(span<const uint8_t>)` | verified MCIL program |
| `run(program)` | verified interpreter execution |
| `run(source)` | compile followed by execution |
| `jit(program)` / `jit(source)` | verified tracing/method JIT execution, with interpreter fallback |
| `parse(input, registry, start)` | committed parser events |
| `parse_ast(input, registry, start)` | DSLtk AST from committed events |
| `rewrite(term_text, rules_text)` | textual term normalization |
| `normalize(term, rules)` | value-level term normalization |
| `parse_term(source)` | verified first-order term |
| `parse_rules(source)` | verified rule list with line/offset diagnostics |
| `assemble(source, object = false)` | raw bytes or ELF object bytes |
| `link(objects, names = {})` | static ELF executable bytes |
| `link_files(paths)` | read and link object files; `IO`/`Link` diagnostics |
| `native_source(program)` | Linux x86-64 assembly text |
| `executable(program)` | lower, assemble, and link |
| `executable(source)` | compile, then lower, assemble, and link |

These methods do not write files or launch the returned executable. `jit` is an
explicit opt-in to in-process native execution; `run` stays interpreted. Use the
linker's explicit file helpers or your application's I/O layer for artifacts.

## DSLtk pipelines

```cpp
metachain::Toolchain tc;
auto result = std::string_view("const r0, 42\nhalt r0\n")
    | tc.compile_stage()
    | tc.run_stage();
```

`compile_stage`, `run_stage`, `native_stage`, `executable_stage`, and
`build_stage` return actual DSLtk `PipeStage` objects. Each owns a copy of the toolchain, so the stage does not
borrow a potentially destroyed `this` pointer. Failures pass through unchanged
instead of executing a later operation with an invalid program.

`compile_stage` and `build_stage` consume text convertible to `string_view`.
`run_stage`, `native_stage`, and `executable_stage` consume
`Outcome<metail::Program>`. For a program already built in C++, call the method
directly or pass its `Outcome` into the corresponding stage.

```cpp
auto elf = std::string_view("const r0, 42\nhalt r0\n") | tc.compile_stage() | tc.executable_stage();
```

`Toolchain` bounds every stage with the same configured `il` limits, including
the verification performed by `run`. The lower-level `metail::run` overload
accepts an explicit `Limits` argument when a host needs to coordinate ceilings.

## Ownership and concurrency

Programs, terms, byte buffers, and ASTs own their data. Source views passed into
a synchronous call need only remain valid during that call. Parser events copy
matched text. `Program::source_map` owns source filenames but not source files.

`link_files` is the only facade method that touches the filesystem. It never
writes a path; combining it with the linker's explicit `write_file` keeps the
read and publish steps visibly separate.

Const toolchain operations use local component instances. Distinct toolchains
or read-only uses of one toolchain can be used concurrently provided caller-
owned arguments are not concurrently modified. A parser, rewrite system, VM,
builder, event stream, or callback collection is not internally synchronized.
Use external synchronization when sharing mutable component instances.
