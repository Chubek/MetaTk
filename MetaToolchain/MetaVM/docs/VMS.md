# Manual for every legacy VM entry

This manual covers all 38 directories under `MetaVM/vmspecs` (there is no `vmspec` directory). It is generated from the catalog and `tools/legacy_profiles.py`. Run `python3 tools/generate_vmspec_manual.py --check` from MetaVM to detect drift.

Five legacy identities have native execution APIs: DVI, FORTH, JVM, Krivine and SECD. The other entries remain unimplemented. Portable example factories are retained for compatibility and do not execute native guest programs. No full language/platform runtime is claimed.

## Build and common usage

```sh
cmake -S MetaVM -B out-vm -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON
cmake --build out-vm -j4
ctest --test-dir out-vm --output-on-failure -j4
out-vm/dvi-run document.dvi
out-vm/closure-vm-demo
```

CMake consumers link `MetaVM::Framework` (VMWeave plus catalog) or `MetaVM::Catalog` for native-only APIs. See [PACKAGING.md](PACKAGING.md) for installation. Legacy C headers work from C11 and C++; native APIs in this manual require C++17.

For a mapped entry, `legacy_NAME_example(&module, &error)` returns an owned portable Machinery module on success. Destroy it with `mvm_module_destroy`. Its example returns 42. Run `.mvm` with `metavm-run`; build real MetaML `.vmweave` with the optional `vmweave-metaml` front end. `.vmwv` legacy companions are Machinery assembly, not native guest specifications. Unmapped factories return `MVM_NOT_FOUND`, clear the output and report a diagnostic. Always check the status before using a module.

VMWeave Builder authors verified Machinery instructions. Definition IR lowers explicit fixed-width definitions with static targets; it cannot infer an ISA from a catalog name. DVI adds verified event replay through the host import `dvi_event`; the native decoder runs before replay. This preserves the difference between guest interpretation and authoring.

## Coverage index

| Legacy name | Catalog identity | Execution status |
|---|---|---|

| ALEX | ALEX | catalog-only |

| BCPL | BCPL | catalog-only |

| BEAM | BEAM | catalog-only |

| BMachine | — | unmapped / unspecified |

| C-Machine | C-Machine | catalog-only |

| CAM | CAM | catalog-only |

| CESK | CESK | catalog-only |

| CLR | CLR | catalog-only |

| CPython | CPython | catalog-only |

| DVI | DVI | dvi2-interpreter |

| FAM | FAM | catalog-only |

| FORTH | FORTH | integer-word-subset |

| FUL | FUL | catalog-only |

| GMachine | — | unmapped / unspecified |

| GraalVM | — | unmapped / unspecified |

| Harrison | Harrison | catalog-only |

| JVM | JVM | integer-method-subset |

| Krivine | Krivine | krivine-whnf-subset |

| LASSIE | — | unmapped / unspecified |

| LuaJIT | — | unmapped / unspecified |

| LuaVM | LuaVM | catalog-only |

| P-Code | P-Code | catalog-only |

| Parrot | ParrotVM | catalog-only |

| Postscript | PostScript | catalog-only |

| Prol | Prol | catalog-only |

| PyPy | — | unmapped / unspecified |

| SECD | SECD | secd-unary-subset |

| SOM | SOM | catalog-only |

| Smalltalk-80 | Smaltalk-80 | catalog-only |

| SynthBE | — | unmapped / unspecified |

| TAM | — | unmapped / unspecified |

| Truffles | — | unmapped / unspecified |

| VMGL | — | unmapped / unspecified |

| VirGL | — | unmapped / unspecified |

| WAM | WAM | catalog-only |

| WASM | WASM | catalog-only |

| YARV | YARV | catalog-only |

| Zend | — | unmapped / unspecified |


## ALEX

Header: `vmspecs/ALEX/ALEX.h`. Compatibility factory: `legacy_ALEX_example`.

Catalog identity: `ALEX`; status: **catalog-only**. `VMLibrary/ALEX/ALEX.vmweave` and `ALEX.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Supply an authoritative ISA, operand semantics and executable examples.


## BCPL

Header: `vmspecs/BCPL/BCPL.h`. Compatibility factory: `legacy_BCPL_example`.

Catalog identity: `BCPL`; status: **catalog-only**. `VMLibrary/BCPL/BCPL.vmweave` and `BCPL.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Select an abstract machine and dialect, word size, memory model and object format.


## BEAM

Header: `vmspecs/BEAM/BEAM.h`. Compatibility factory: `legacy_BEAM_example`.

Catalog identity: `BEAM`; status: **catalog-only**. `VMLibrary/BEAM/BEAM.vmweave` and `BEAM.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Pin an OTP release, instruction definitions, BEAM container and scheduler/runtime contracts.


## BMachine

Header: `vmspecs/BMachine/BMachine.h`. Compatibility factory: `legacy_BMachine_example`.

Status: **unmapped / unspecified**. The compatibility factory returns `MVM_NOT_FOUND`; the `.vmwv` companion has no guest definition. No portable or native success is fabricated.

Compatibility requirement: Identify this machine and supply its transition rules or ISA; no catalog mapping exists.


## C-Machine

Header: `vmspecs/C-Machine/C-Machine.h`. Compatibility factory: `legacy_C_Machine_example`.

Catalog identity: `C-Machine`; status: **catalog-only**. `VMLibrary/C-Machine/C-Machine.vmweave` and `C-Machine.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Identify this machine and supply its transition rules, value representation and examples.


## CAM

Header: `vmspecs/CAM/CAM.h`. Compatibility factory: `legacy_CAM_example`.

Catalog identity: `CAM`; status: **catalog-only**. `VMLibrary/CAM/CAM.vmweave` and `CAM.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Select a categorical abstract machine presentation, instruction set and value representation.


## CESK

Header: `vmspecs/CESK/CESK.h`. Compatibility factory: `legacy_CESK_example`.

Catalog identity: `CESK`; status: **catalog-only**. `VMLibrary/CESK/CESK.vmweave` and `CESK.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Select a source calculus, evaluation strategy, store allocation and continuation rules.


## CLR

Header: `vmspecs/CLR/CLR.h`. Compatibility factory: `legacy_CLR_example`.

Catalog identity: `CLR`; status: **catalog-only**. `VMLibrary/CLR/CLR.vmweave` and `CLR.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Select an ECMA-335 edition, CIL scope, assembly loading, metadata and managed-runtime requirements.


## CPython

Header: `vmspecs/CPython/CPython.h`. Compatibility factory: `legacy_CPython_example`.

Catalog identity: `CPython`; status: **catalog-only**. `VMLibrary/CPython/CPython.vmweave` and `CPython.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Pin a Python bytecode release and specify code-object, object-model and runtime compatibility.


## DVI

Header: `vmspecs/DVI/DVI.h`. Compatibility factory: `legacy_DVI_example`.

Native API: `DviMachine::run(bytes, metrics[, limits])`. Read the [execution contract](DVI.md) for instructions, errors, ownership and resource limits.

Compatibility boundaries: Standard DVI version 2; all defined opcode families, structural validation and integer-coordinate event output; see docs/DVI.md. SET characters require caller-supplied scaled font metrics; no font discovery, rasterization, driver-specific special semantics, XDV or pTeX extensions.

Compatibility requirement: Standard DVI identifier 2 is implemented; supply scaled glyph advances and a rendering/specials policy.


## FAM

Header: `vmspecs/FAM/FAM.h`. Compatibility factory: `legacy_FAM_example`.

Catalog identity: `FAM`; status: **catalog-only**. `VMLibrary/FAM/FAM.vmweave` and `FAM.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Identify the intended FAM and supply an authoritative ISA and executable examples.


## FORTH

Header: `vmspecs/FORTH/FORTH.h`. Compatibility factory: `legacy_FORTH_example`.

Native API: `metavm::machines::forth(source, fuel, stack_limit)`. Read the [execution contract](VMS_NATIVE.md) for instructions, errors, ownership and resource limits.

Compatibility boundaries: See native API header for exact implemented subset. No full platform runtime or file loader.

Compatibility requirement: The current i64 word subset is executable; select a Forth standard and environment for expansion.


## FUL

Header: `vmspecs/FUL/FUL.h`. Compatibility factory: `legacy_FUL_example`.

Catalog identity: `FUL`; status: **catalog-only**. `VMLibrary/FUL/FUL.vmweave` and `FUL.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Supply an authoritative machine definition, operand semantics and executable examples.


## GMachine

Header: `vmspecs/GMachine/GMachine.h`. Compatibility factory: `legacy_GMachine_example`.

Status: **unmapped / unspecified**. The compatibility factory returns `MVM_NOT_FOUND`; the `.vmwv` companion has no guest definition. No portable or native success is fabricated.

Compatibility requirement: Select a G-machine presentation, graph representation, update rules and compiler interface.


## GraalVM

Header: `vmspecs/GraalVM/GraalVM.h`. Compatibility factory: `legacy_GraalVM_example`.

Status: **unmapped / unspecified**. The compatibility factory returns `MVM_NOT_FOUND`; the `.vmwv` companion has no guest definition. No portable or native success is fabricated.

Compatibility requirement: Specify the intended guest language/format, runtime release and interoperability contract.


## Harrison

Header: `vmspecs/Harrison/Harrison.h`. Compatibility factory: `legacy_Harrison_example`.

Catalog identity: `Harrison`; status: **catalog-only**. `VMLibrary/Harrison/Harrison.vmweave` and `Harrison.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Identify this machine and supply its transition rules or ISA and executable examples.


## JVM

Header: `vmspecs/JVM/JVM.h`. Compatibility factory: `legacy_JVM_example`.

Native API: `metavm::machines::jvm_integer_method(code, args, locals, stack_limit, fuel)`. Read the [execution contract](VMS_NATIVE.md) for instructions, errors, ownership and resource limits.

Compatibility boundaries: See native API header for exact implemented subset. No full platform runtime or file loader.

Compatibility requirement: The current i32 Code-array subset is executable; select a JVMS edition and class/runtime compatibility target.


## Krivine

Header: `vmspecs/Krivine/Krivine.h`. Compatibility factory: `legacy_Krivine_example`.

Native API: `KrivineMachine::verify / KrivineMachine::run`. Read the [execution contract](CLOSURE_VMS.md) for instructions, errors, ownership and resource limits.

Compatibility boundaries: MetaVM in-memory closed-term Krivine profile v1; see docs/CLOSURE_VMS.md. Weak-head call-by-name only; no binary loader, full normalization, call-by-need memoization, control operators or GC.

Compatibility requirement: The closed-term, weak-head call-by-name profile v1 is executable; specify any requested extensions.


## LASSIE

Header: `vmspecs/LASSIE/LASSIE.h`. Compatibility factory: `legacy_LASSIE_example`.

Status: **unmapped / unspecified**. The compatibility factory returns `MVM_NOT_FOUND`; the `.vmwv` companion has no guest definition. No portable or native success is fabricated.

Compatibility requirement: Supply an authoritative machine definition and executable examples; no catalog mapping exists.


## LuaJIT

Header: `vmspecs/LuaJIT/LuaJIT.h`. Compatibility factory: `legacy_LuaJIT_example`.

Status: **unmapped / unspecified**. The compatibility factory returns `MVM_NOT_FOUND`; the `.vmwv` companion has no guest definition. No portable or native success is fabricated.

Compatibility requirement: Pin a LuaJIT release, bytecode dump format and runtime/FFI scope; no catalog mapping exists.


## LuaVM

Header: `vmspecs/LuaVM/LuaVM.h`. Compatibility factory: `legacy_LuaVM_example`.

Catalog identity: `LuaVM`; status: **catalog-only**. `VMLibrary/LuaVM/LuaVM.vmweave` and `LuaVM.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Pin a Lua release, binary chunk format, number representation and object-runtime scope.


## P-Code

Header: `vmspecs/P-Code/P-Code.h`. Compatibility factory: `legacy_P_Code_example`.

Catalog identity: `P-Code`; status: **catalog-only**. `VMLibrary/P-Code/P-Code.vmweave` and `P-Code.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Select a Pascal p-code dialect, instruction encoding, memory layout and runtime interface.


## Parrot

Header: `vmspecs/Parrot/Parrot.h`. Compatibility factory: `legacy_Parrot_example`.

Catalog identity: `ParrotVM`; status: **catalog-only**. `VMLibrary/ParrotVM/ParrotVM.vmweave` and `ParrotVM.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Pin a Parrot release, packfile format, op libraries and object/runtime behavior.


## Postscript

Header: `vmspecs/Postscript/Postscript.h`. Compatibility factory: `legacy_Postscript_example`.

Catalog identity: `PostScript`; status: **catalog-only**. `VMLibrary/PostScript/PostScript.vmweave` and `PostScript.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Select a PostScript language level, operators, fonts and graphics-device contract.


## Prol

Header: `vmspecs/Prol/Prol.h`. Compatibility factory: `legacy_Prol_example`.

Catalog identity: `Prol`; status: **catalog-only**. `VMLibrary/Prol/Prol.vmweave` and `Prol.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Identify this machine and supply its transition rules or ISA and executable examples.


## PyPy

Header: `vmspecs/PyPy/PyPy.h`. Compatibility factory: `legacy_PyPy_example`.

Status: **unmapped / unspecified**. The compatibility factory returns `MVM_NOT_FOUND`; the `.vmwv` companion has no guest definition. No portable or native success is fabricated.

Compatibility requirement: Specify the intended bytecode/IR, release, Python language scope and runtime behavior.


## SECD

Header: `vmspecs/SECD/SECD.h`. Compatibility factory: `legacy_SECD_example`.

Native API: `SECDMachine::verify / SECDMachine::run`. Read the [execution contract](CLOSURE_VMS.md) for instructions, errors, ownership and resource limits.

Compatibility boundaries: MetaVM in-memory unary/i64 SECD profile v1; see docs/CLOSURE_VMS.md. No Lispkit binary loader, lists, DUM/RAP, mutable recursive environments, I/O or GC; cumulative environment budgets apply.

Compatibility requirement: The unary/i64 lexical-closure profile v1 is executable; specify lists, recursive environments or image formats if needed.


## SOM

Header: `vmspecs/SOM/SOM.h`. Compatibility factory: `legacy_SOM_example`.

Catalog identity: `SOM`; status: **catalog-only**. `VMLibrary/SOM/SOM.vmweave` and `SOM.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Pin a SOM implementation, instruction set, source/image format and object primitives.


## Smalltalk-80

Header: `vmspecs/Smalltalk-80/Smalltalk-80.h`. Compatibility factory: `legacy_Smalltalk_80_example`.

Catalog identity: `Smaltalk-80`; status: **catalog-only**. `VMLibrary/Smaltalk-80/Smaltalk-80.vmweave` and `Smaltalk-80.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Select an image format, bytecode dialect, object memory, primitives and host-device interface.


## SynthBE

Header: `vmspecs/SynthBE/SynthBE.h`. Compatibility factory: `legacy_SynthBE_example`.

Status: **unmapped / unspecified**. The compatibility factory returns `MVM_NOT_FOUND`; the `.vmwv` companion has no guest definition. No portable or native success is fabricated.

Compatibility requirement: Supply an authoritative machine definition and executable examples; no catalog mapping exists.


## TAM

Header: `vmspecs/TAM/TAM.h`. Compatibility factory: `legacy_TAM_example`.

Status: **unmapped / unspecified**. The compatibility factory returns `MVM_NOT_FOUND`; the `.vmwv` companion has no guest definition. No portable or native success is fabricated.

Compatibility requirement: Identify this machine and supply its ISA; it is not silently aliased to TAMMY.


## Truffles

Header: `vmspecs/Truffles/Truffles.h`. Compatibility factory: `legacy_Truffles_example`.

Status: **unmapped / unspecified**. The compatibility factory returns `MVM_NOT_FOUND`; the `.vmwv` companion has no guest definition. No portable or native success is fabricated.

Compatibility requirement: Specify the intended machine or language implementation and version; no catalog mapping exists.


## VMGL

Header: `vmspecs/VMGL/VMGL.h`. Compatibility factory: `legacy_VMGL_example`.

Status: **unmapped / unspecified**. The compatibility factory returns `MVM_NOT_FOUND`; the `.vmwv` companion has no guest definition. No portable or native success is fabricated.

Compatibility requirement: Supply an authoritative command format, version, graphics semantics and host-device contract.


## VirGL

Header: `vmspecs/VirGL/VirGL.h`. Compatibility factory: `legacy_VirGL_example`.

Status: **unmapped / unspecified**. The compatibility factory returns `MVM_NOT_FOUND`; the `.vmwv` companion has no guest definition. No portable or native success is fabricated.

Compatibility requirement: Specify the intended wire protocol/version, command semantics, resources and renderer interface.


## WAM

Header: `vmspecs/WAM/WAM.h`. Compatibility factory: `legacy_WAM_example`.

Catalog identity: `WAM`; status: **catalog-only**. `VMLibrary/WAM/WAM.vmweave` and `WAM.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Select a WAM presentation, instruction encoding, term/store model and Prolog runtime scope.


## WASM

Header: `vmspecs/WASM/WASM.h`. Compatibility factory: `legacy_WASM_example`.

Catalog identity: `WASM`; status: **catalog-only**. `VMLibrary/WASM/WASM.vmweave` and `WASM.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Select a WebAssembly specification version, feature set, module loading and host-import contract.


## YARV

Header: `vmspecs/YARV/YARV.h`. Compatibility factory: `legacy_YARV_example`.

Catalog identity: `YARV`; status: **catalog-only**. `VMLibrary/YARV/YARV.vmweave` and `YARV.mvm` are portable examples. There is no native execution API or guest loader.

Compatibility requirement: Pin a Ruby release, instruction-sequence format and object/runtime compatibility.


## Zend

Header: `vmspecs/Zend/Zend.h`. Compatibility factory: `legacy_Zend_example`.

Status: **unmapped / unspecified**. The compatibility factory returns `MVM_NOT_FOUND`; the `.vmwv` companion has no guest definition. No portable or native success is fabricated.

Compatibility requirement: Pin a PHP/Zend release, opcode representation and value/runtime behavior.
