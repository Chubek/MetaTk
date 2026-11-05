> **XVM is now the default Machinery executor.** It compiles verified programs
> to portable/native helper-call blocks while retaining both previous engines.
> Current MetaML is also compiled by default. See [XVM.md](docs/XVM.md) for APIs,
> tier selection, resource limits and unchanged catalog/runtime boundaries.

**[Read the complete 40-chapter manual](manual/README.md)** for build instructions,
API contracts, generation workflows, native profiles, and runnable SDK examples.

> **Generation repair:** The build now generates verified binary modules and C
> factories for all 49 portable catalog examples. See [GENERATION.md](docs/GENERATION.md)
> for commands, failure guarantees and the distinction from native guest runtimes.
>
> **Current refinement:** [VMS.md](docs/VMS.md) is the manual for all 38 legacy
> `vmspecs` entries, with exact implementation status and compatibility requirements.
> [DVI.md](docs/DVI.md) documents the new standard DVI version-2 decoder,
> interpreter, `dvi-run` and VMWeave/Machinery event replay. The 49-entry catalog
> now has eight native execution profiles and 41 catalog-only entries; five native
> profiles correspond to legacy names. The request to implement every VM remains
> incomplete. Unspecified ISAs and unpinned runtime versions are listed explicitly.
> CMake builds and the relocated SDK are exercised; see [REPORT.md](REPORT.md).
> Historical implementation notes below describe earlier passes.
>
> **Previous implementation:** [ROUND6.md](docs/ROUND6.md) adds executable SECD and
> Krivine closure-machine subsets. There are now seven native subsets and 42
> catalog-only profiles. See [CLOSURE_VMS.md](docs/CLOSURE_VMS.md) for their APIs,
> semantics and limits; run `build/closure-vm-demo` for a native example.
>
> **Fifth implementation pass:** [ROUND5.md](docs/ROUND5.md) adds bounded, opt-in
> SSAVM definite-assignment/type analysis and `ssavm-run --check` with source-line
> diagnostics. See [SSAVM.md](docs/SSAVM.md) for guarantees and limits.
>
> **Fourth implementation pass:** [ROUND4.md](docs/ROUND4.md) adds documented CHIP-8
> presets, sampled key/vblank waits, pinned ROM regressions and a relocatable SDK.
> See [CHIP8.md](docs/CHIP8.md) and [PACKAGING.md](docs/PACKAGING.md).
>
> **Third implementation pass:** [ROUND3.md](docs/ROUND3.md) documents the tested real
> MetaML/Definition embedding and expanded, resource-bounded SSAVM. CMake and
> compatible DSLtk are now available; historical dependency limitations below
> are superseded by that record. See [SSAVM.md](docs/SSAVM.md) for its API and CLI.
>
> **Second implementation pass:** see [ROUND2.md](docs/ROUND2.md) for the prior
> layer map, definition lowering, SSA source runner, atomic APIs and verification.
> The first-edition text below is retained; resolved gaps are superseded there.
> Every old source path remains, and `history/round1.zip` preserves its exact bytes.

# MetaVM

MetaVM is the VM framework for MetaTks / MetaToolchain. This working development
edition builds the supplied C11 Machinery engine, a VMWeave assembly bootstrap
and C emitter, a C++ authoring builder, a 49-machine catalog and five executable
native subsets. `AGENTS.md` is the implementation/refinement contract.

VMWeave embeds the supplied MetaML through its real Session API. VMDesc embeds
the supplied MetaTeX through its Engine API. Both adapters are implemented as
optional integrations; their external dependency graphs are not available here.
In particular, the supplied MetaML header includes an absent `DSLtk.hpp`.

## Build and use

C11/C++17 compilers, Python 3, and `ar` are enough for the fallback build:

```sh
make test
build/metavm-run Machinery/examples/factorial.mvm
build/vmweave compile VMLibrary/JVM/JVM.mvm example.mvmb
build/metavm-run example.mvmb
build/vmweave emit-c VMLibrary/JVM/JVM.mvm example.c example_factory
```

The first run returns 3628800; the portable example returns 42. Generated C
contains a serialized verified module and an owned-module factory; link it with
Machinery. The factory does not itself execute the module.

CMake >= 3.18 is also supported in source, but could not be exercised here:

```sh
cmake -S . -B out -DMETAVM_TEST_CPP=ON
cmake --build out -j
ctest --test-dir out --output-on-failure
```

Use `METAVM_ENABLE_METAML=ON` and `METAVM_DSLTK_INCLUDE_DIR` to build
`vmweave-metaml`. It compiles `.vmweave` through MetaML into Machinery modules.
The dependency-free `vmweave` executable accepts Machinery assembly/binary
modules; it does not parse MetaML. See `vmweave/factorial.vmweave` for the typed
embedding API and `vmweave/Builder.hpp` for its tested C++ counterpart.

Use `METAVM_ENABLE_VMDESC=ON` with `metatex_core` already provided by a parent
project, or set `METAVM_METATEX_SOURCE_DIR` to the extracted supplied project.
Its MetaVect, DSLtk and other dependencies must be configured as MetaTeX requires.
The VMDesc CLI emits the compiled document text; richer renderers remain MetaTeX's.

## Contents and real coverage

| Component | Current implementation |
|---|---|
| Machinery | Supplied C11 execution engine, verification, objects, GC, host imports and serialization |
| VMWeave | Assembly bootstrap, C emitter, typed builder; optional actual MetaML adapter |
| VMDesc | Actual MetaTeX command-registration adapter; integration not yet validated |
| CHIP-8 | Classic opcode interpreter, display XOR/collision, keyboard state, timers and quirk switches |
| SSAVM | ISSA result registers, edge selection, simultaneous phi groups and immutable array versions |
| DRMVM | Graphics through Machinery host imports; software surface and optional libdrm dumb-buffer upload |
| JVM | Bounded integer Code-array interpreter; no class loader or object runtime |
| FORTH | Integer stack words and whole-input colon definitions; no full standard environment |
| Other 44 profiles | Descriptors and explicit portable Machinery examples; native execution unimplemented |

Every `VMLibrary/<machine>/` contains C/H files, C++ catalog adapters, `.vmweave`,
`.vmspc`, `.vmdesc`, a portable `.mvm` example, and build/doc configuration. Native
APIs are in additional named headers (`Chip8.hpp`, `ISSA.hpp`, `Graphics.hpp`,
`IntegerMethod.hpp`, `Forth.hpp`). The `.vmspc` files and `MANIFEST.yaml` use JSON,
a YAML-compatible representation, so standard Python can read them without PyYAML.
The native implementations are currently hand-written, not generated from
architecture definitions in VMWeave. That lowering stage is a refinement priority.

`tools/generate_catalog.py` regenerates catalog-owned files only. Legacy
`vmspecs/` files expose clearly named portable-example adapters, with a few
spelling aliases. Unmatched legacy profiles report `MVM_NOT_FOUND`.

SSAVM follows the attached TR 03-19 operational model for CEN and PHI/PFE.
The persistent-copy array implementation preserves old versions but does not
reproduce the paper's optimized storage representation. See `docs/vm-ssa.pdf`.

DRMVM runs normal Machinery modules that import `drm_clear(color)` and
`drm_rect(x,y,width,height,color)`. Bind a `Graphics` instance to the VM before
execution; it must outlive that VM. A compositor owns presentation. Optional
`DumbBuffer` uses a caller-provided KMS fd; there is no automatic modesetting.
Enable it with `METAVM_ENABLE_LIBDRM=ON` after installing libdrm development files.

Read `docs/COVERAGE.md` for every profile, `docs/VALIDATION.md` for observed tests,
and `AGENTS.md` for exact invariants, known gaps and prioritized next work.
No full JVM, CLR, WebAssembly, Android, JavaScript, Python or other platform
runtime is claimed by this development edition.
