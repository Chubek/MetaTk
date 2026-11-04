# Generation pipeline repair

## Build and generate

Run these commands from `MetaToolchain/MetaVM`:

```sh
cmake -S . -B out-generation -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON
cmake --build out-generation -j4
ctest --test-dir out-generation --output-on-failure
```

`METAVM_GENERATE_EXAMPLES=ON` (the default) builds the
`metavm-generated-examples` target. Python 3 and the built VMWeave compiler
translate each manifest-listed `.mvm` into a verified `.mvmb`, then emit C from
that binary. Outputs are under `out-generation/generated/VMLibrary/<machine>/`.
Build dependencies use that same manifest inventory; unrelated assembly files
are not accidentally treated as generated examples.
`catalog_factories.hpp` lists the emitted factory names for C++ consumers.
Set the option OFF to omit these build artifacts and the associated execution
test. Python is not a runtime dependency of emitted modules or C factories.

Standalone generation with an already-built compiler is also available:

```sh
python3 tools/generate_examples.py --vmweave out-generation/vmweave \
  --output-root out-generation/other-examples
```

Names are `generated_<manifest-symbol>_example_module`. Link generated C with
Machinery; each successful factory returns a caller-owned module. A factory
neither allocates a VM nor runs the module. Modules retain their host imports:
the DRMVM example requires the graphics bindings. Factories use Machinery's
default module-load limits. No limits are silently raised.

Compilation/emission of the whole set completes in a temporary directory before
publishing any destination artifact. Validation/compiler failures leave existing
outputs untouched. Publication uses individual atomic replacements, not a
transaction across the complete directory: an I/O failure during publication can
leave a mixed set. There is no fsync durability guarantee. Unchanged artifacts are
not rewritten by the script. CMake touches its declared build outputs after a
successful rule to keep dependency timestamps current. The source root itself
is rejected as a compiled-example output directory, protecting catalog C sources.

## Catalog source regeneration

```sh
python3 tools/generate_catalog.py --check
python3 tools/generate_catalog.py
python3 tools/generate_catalog.py --output-root out-generation/catalog-source
python3 tools/generate_catalog.py --output-root out-generation/catalog-source --check
```

`--check` reports every stale/missing owned file and exits 1 without changing
anything. Generation renders/validates the full set before writing, rejects
invalid identities and symbol/header-guard collisions, preserves unchanged
files/mtimes, and replaces changed files individually. The alternate output
root receives generated files only, not copies of native implementations.
Unrelated files, including empty build artifacts, are ignored. Native headers
referenced by generated adapters must exist in the source catalog.

The root Makefile's `regenerate` target regenerates catalog source metadata;
`make` and `make test` also generate the compiled examples in `build/generated`.
Legacy unknown-machine adapters now follow their generator's existing contract:
clear the output pointer, populate diagnostics, return `MVM_NOT_FOUND` for an
unsupported identity and `MVM_INVALID` for a null output pointer.

## What is and is not generated

- The 49 catalog `.mvm` programs and C factories are portable Machinery examples.
  Successful execution is not evidence of historical guest compatibility.
- `vmweave/Definition.hpp` still supplies verified, program-specialized lowering
  of the explicitly defined fixed-width guest ISA. The ToyR16 demo and emitted-C
  execution regressions remain intact. It is not a reusable native decoder.
- Real MetaML authoring remains optional and uses the supplied Session API;
  no lookalike parser or stub dependency was introduced.
- Eight profiles have separate native implementations; 41 remain catalog-only.
  This repair does not invent missing ISAs or turn metadata into native semantics.
- The missing `vmweave/DviReplay.hpp` now implements the existing DVI replay
  contract: owned document/sink, nonmovable callback state, checked event indices,
  exception-safe C callbacks and bounded verified-module generation. It replays
  decoded events; it does not generate a DVI decoder. See [DVI.md](DVI.md).

## Baseline and regressions

The initial CMake configuration failed because required `vmweave/` sources had
been deleted. After the user-authorized restoration, compilation exposed the
missing DVI replay header. Once supplied, the suite exposed stale legacy adapters
whose error/output behavior differed from the current generator. Regeneration
repaired those adapters without changing native instruction semantics.

New coverage compiles all 49 emitted C factories, compares their serialized
modules against both assembly companions and catalog factories, reloads each
binary and executes every generated module under both Machinery engines.
Stream and atomic-file C emission must agree byte-for-byte. Regeneration tests
cover stale/missing outputs, no-op mtimes, native-file preservation, unrelated
empty files, identity collisions and a real compiler failure after a previously
successful first input. Existing 5,056 Machinery checks and native suites remain.

Observed Linux validation uses GCC 16.2.1, Clang 22.1.8 and warnings as errors:

| Configuration | Result |
|---|---|
| GCC Release, C++11 wrapper | 22/22 CTest cases passed |
| Clang Release, real MetaML, C++11 wrapper | 24/24 passed |
| Clang Debug, ASan/UBSan, C++11 wrapper | 22/22 passed with default leak detection |
| GCC fallback `python3 tools/build.py --test` | All stages passed |
| GCC fallback ASan/UBSan | All stages passed with default leak detection |
| GCC Release shared libraries, relocated `lib64` SDK | 22/22 passed |
| Tests, example generation and Machinery runner disabled | Build passed |

The sanitizer configuration does not enable MetaML; no claim is made that it
resolves the dependency's previously documented type-checker leaks. SDK relocation
and public-header tests are included. MetaTeX/libdrm and non-Linux validation
remain separate work. Timings from concurrent test runs are not benchmarks.
