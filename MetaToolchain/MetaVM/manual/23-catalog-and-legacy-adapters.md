# Chapter 23 — The VMLibrary catalog and legacy adapters

[Previous](22-metaml-authoring.md) · [Contents](README.md) · [Next](24-generation-workflows.md)

## Registry and metadata

`VMLibrary/Library.hpp` declares `metavm::library::Profile`, `catalog()`, and
`find(std::string_view)`. Each Profile stores name, architecture, coverage,
description, and a portable example factory pointer. `find` performs an exact
catalog-name lookup and returns null when absent. It does not select a native
interpreter or apply every legacy alias.

```cpp
const auto* profile = metavm::library::find("JVM");
if (!profile) throw std::runtime_error("missing profile");
mvm_module* module = nullptr;
mvm_error error{};
metavm::check(profile->example_module(&module, &error), error);
// Caller destroys module; this is the portable example, not Java execution.
```

`MANIFEST.yaml` and per-machine `.vmspc` use JSON, a YAML-compatible form that
the Python tools read without PyYAML. Schema `metavm.machine/1` describes
identity/coverage/companion paths, not an executable ISA. Relevant fields include
`symbol`, `architecture`, `native_coverage`, `native_format_loader`,
`portable_example`, `vmweave`, `vmspc`, `vmdesc`, and `limitations`.

## Complete catalog inventory

“Catalog-only” means no native execution implementation. The eight other entries
are still bounded subsets, not full platform runtimes.

| Profile | Coverage |
|---|---|
| ALEX | Catalog-only |
| ART | Catalog-only |
| AVM2 | Catalog-only |
| B-Code | Catalog-only |
| BCPL | Catalog-only |
| BEAM | Catalog-only |
| C-Machine | Catalog-only |
| CAM | Catalog-only |
| CESK | Catalog-only |
| CHIP-8 | Classic-opcode interpreter |
| CLR | Catalog-only |
| CPython | Catalog-only |
| ConcurrentML | Catalog-only |
| DRMVM | Graphics host-import subset |
| DVI | Standard DVI version-2 interpreter |
| Dalvik | Catalog-only |
| Dart | Catalog-only |
| Dis | Catalog-only |
| FAM | Catalog-only |
| FORTH | Integer-word subset |
| FUL | Catalog-only |
| G-Code | Catalog-only |
| Harrison | Catalog-only |
| IKL | Catalog-only |
| JSCore | Catalog-only |
| JVM | Integer Code-array subset |
| Krivine | Weak-head call-by-name profile |
| LuaVM | Catalog-only |
| Lucide | Catalog-only |
| Monty | Catalog-only |
| MoscowML | Catalog-only |
| P-Code | Catalog-only |
| P4 | Catalog-only |
| ParrotVM | Catalog-only |
| PostScript | Catalog-only |
| Prol | Catalog-only |
| SCUMM | Catalog-only |
| SECD | Unary/i64 closure-machine profile |
| SOM | Catalog-only |
| SSAVM | i64 scalar-array ISSA subset |
| Self | Catalog-only |
| Smaltalk-80 | Catalog-only; historical directory spelling retained |
| TAMMY | Catalog-only |
| V8 | Catalog-only |
| WAM | Catalog-only |
| WASM | Catalog-only |
| YARV | Catalog-only |
| Z-Machine | Catalog-only |
| Zand | Catalog-only |

## Companion files and factory ownership

Each generated profile has `.c/.h`, `.cpp/.hpp`, `.mvm`, `.vmweave`, `.vmspc`,
and `.vmdesc` companions plus build/documentation configuration. A C factory
`<symbol>_example_module` compiles the portable assembly example and returns
an owned verified Machinery module. Its result `42` establishes plumbing only.
DRMVM's example additionally requires graphics imports.

## Legacy `vmspecs` identities

The 38 legacy directories expose `legacy_<symbol>_example`, not native loaders.
Three explicit aliases are `Smalltalk-80 → Smaltalk-80`,
`Postscript → PostScript`, and `Parrot → ParrotVM`. Other mapped entries use
their exact catalog name. Five legacy identities also have separate native APIs:
DVI, FORTH, JVM, Krivine, and SECD.

Unmapped entries are BMachine, GMachine, GraalVM, LASSIE, LuaJIT, PyPy, SynthBE,
TAM, Truffles, VMGL, VirGL, and Zend. Their factories clear the output and return
`MVM_NOT_FOUND` with a diagnostic; a null output pointer returns `MVM_INVALID`.
In particular, TAM is not guessed to be TAMMY, and a graphics-sounding name
is not silently redirected to DRMVM.

**References:** [generated coverage](../docs/COVERAGE.md), [all legacy requirements](../docs/VMS.md), [schema](../docs/machine.schema.json).
