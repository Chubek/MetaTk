# DomSymAsCC stdspec

`.dsymspec` files are architecture specifications written in the
DomSymAsCC DSL (a small Lua library run under the embedded DomMune
interpreter). Each file describes one architecture's registers, extensions,
relocations, sections, and a representative instruction set, and prints a
JSON document with assembler records and a structured `backend` extension
for register allocation, instruction selection, and instruction scheduling.
`domlibs/domsymas/compiler` maps the assembler records onto
`domsymas::Builder` to emit a SymAS databank (`.symas`). Compiler consumers
read the `backend` extension from the emitted JSON; the existing SymAS
builder does not persist that extension.

```text
  .dsymspec (Lua DSL)  ->  JSON document  ->  domsymas::Builder  ->  .symas
       (DomMune)             (this file)        (spec_document.cpp)
```

## Using the DSL

```lua
local symas = require("dsl").new()

symas.target{ arch = "riscv64", addr_size = 64, byte_order = "little", ... }
symas.feature{ name = "m", desc = "integer multiply/divide" }
symas.register{ name = "x0", class = "gpr", width = 64, enc = 0,
                flags = { "readable", "writable" } }
symas.encoder{ name = "add_rr", source = "emit32le(0x0033 | (rd << 7) | (rs1 << 15) | (rs2 << 20));" }
symas.mnemonic{ name = "add", ci = false }
symas.form{ mnemonic = "add", encoder = "add_rr", ops = 3,
            operands = { { kind = "reg", class = "gpr" },
                         { kind = "reg", class = "gpr" },
                         { kind = "reg", class = "gpr" } } }
symas.syntax{ name = "default", comment = "#" }
symas.section{ name = ".text", flags = { "exec", "alloc", "code" }, align = 4,
               content = "code" }
symas.reloc{ name = "R_RISCV_CALL", width = 32, signed = true, pcrel = true,
             addend = "explicit", expr = "S + A - P" }

print(symas.emit())
```

`require("dsl")` returns a module; `dsl.new()` returns a fresh builder so
multiple spec files can be compiled into one databank without sharing state.
`emit()` performs a referential-integrity check (unknown mnemonic/encoder/
feature/syntax names are rejected) before serializing.

## JSON document schema

The document is a single object. `schema` is always `1`. Records reference
one another by **name**; the C++ compiler resolves names to numeric ids.

```jsonc
{
  "schema": 1,
  "targets":    [ Target ],
  "features":   [ Feature ],
  "registers":  [ Register ],
  "mnemonics":  [ Mnemonic ],
  "encoders":   [ Encoder ],
  "forms":      [ Form ],
  "syntaxes":   [ Syntax ],
  "sections":   [ Section ],
  "linkages":   [ Linkage ],
  "relocs":     [ Reloc ],
  "debug":      [ Debug ]
}
```

### Target

```jsonc
{ "arch": "riscv64", "subarch": "", "addr_size": 64, "op_size": 32,
  "addr_def": 64, "align": 2, "byte_order": "little", "obj_format": "ELF64",
  "abi": "LP64D", "calling_conv": "lp64d", "features": ["i", "m", "c"] }
```

`byte_order` is `"little"`/`"le"` or `"big"`/`"be"`. `align` must be a power
of two. `features` lists feature **names** declared in the `features` array.

### Feature

```jsonc
{ "name": "m", "desc": "integer multiply/divide", "flags": 0 }
```

### Register

```jsonc
{ "name": "w0", "class": "gpr", "width": 32, "enc": 0, "parent": "x0",
  "sub_lo": 0, "sub_hi": 32, "flags": ["readable", "writable"] }
```

`class` ∈ `gpr seg ctrl debug vec mask fp status special`. `parent` is a
register **name** (or absent). `width` ∈ `{8,16,32,64,80,128,256,512}`.
`flags` ∈ `readable writable imp_used imp_mod modedep`.

### Mnemonic

```jsonc
{ "name": "add", "aliases": ["ADD"], "ci": false }
```

`ci` sets the case-insensitive bit.

### Encoder

```jsonc
{ "name": "add_rr", "source": "emit32le(...);", "flags": 0,
  "max_bytes": 4, "max_relocs": 0 }
```

`source` is the Algol-like encoding language parsed by
`domlibs/domsymas` (`domdsl`). Statements: `emit8`, `emit16le/be`,
`emit32le/be`, `emit64le/be`, `emit_imm(width,v)`, `emit_disp(width,v)`,
`emit_modrm(dst,src)`, `emit_sib(a,b,c)`, `emit_relocation(sym,TYPE)`,
`emit_reloc`, `require(cond);`, `if (...) {...} else {...}`, `fail("msg")`.
Operands are referenced by name (`rd`, `rs1`, `imm`, ...).

### Form (with inline operands)

```jsonc
{ "mnemonic": "add", "ops": 3, "encoder": "add_rr",
  "flags": [], "required": [], "forbidden": [], "syntaxes": [],
  "operands": [ Operand, Operand, Operand ] }
```

`ops` defaults to `#operands`. `flags` ∈ `privileged branch cond_branch
return terminator may_trap reads_mem writes_mem delayed implicit_ops reloc
aliases`. `required`/`forbidden` list feature names; `syntaxes` lists syntax
names.

### Operand

```jsonc
{ "kind": "reg", "width": 64, "class": "gpr", "imm_min": 0, "imm_max": 0,
  "signed": false, "role": 0, "access": "read", "hidden": false,
  "constraint": "" }
```

`kind` ∈ `reg imm disp mem abs rel sym cond reglist implicit`. `access` ∈
`read write readwrite`. For `kind == "reg"` the `class` may name a register
class.

### Syntax

```jsonc
{ "name": "default", "order": 0, "reg_prefix": "", "imm_prefix": "",
  "comment": "#", "separator": ", ", "tmpl": "{mnemonic} {ops}", "ci": false }
```

`order` `1` reverses operand order (AT&T-style).

### Section / Linkage / Reloc / Debug

```jsonc
{ "name": ".text", "flags": ["exec", "alloc", "code"], "align": 4,
  "addrspace": 0, "content": "code" }

{ "name": "global_func", "binding": "global", "visibility": "default",
  "symtype": "func", "flags": 0 }

{ "name": "R_RISCV_CALL", "width": 32, "offset": 0, "signed": true,
  "pcrel": true, "overflow": "error", "addend": "explicit", "expr": "S + A - P" }

{ "msg": "...", "kind": "form", "ref": "add" }
```

`section.flags` ∈ `exec write alloc readonly zerofill merge discard tls debug
code data meta`; `content` ∈ `unknown code data meta`. `reloc.overflow` ∈
`error truncate`; `reloc.addend` ∈ `explicit infield implied`. `debug.kind` ∈
`target register feature mnemonic form operand encoder syntax section linkage
reloc`.

## Layout

Each file in this directory (`<arch>.dsymspec`) uses an architecture key.
The original register files, register classes, ABI
metadata, extensions, and target triples are drawn from
`domainlet/bekern/arch/_arch_specs.py`; instruction encodings follow the
corresponding ISA manuals and are cross-checked with the `arch_oracle` /
`isa_reference` MCP tools.

## Compiler backend extension

All 28 specifications load `require("backend").new("<arch>")`.
`backend.lua` delegates to the existing DSL builder and adds the generated
JSON object from `backend/<arch>.lua` to `emit()`. This preserves JSON
`null`, empty arrays, and structured TableGen DAGs without requiring file
I/O or a JSON parser in DomMune. Put this directory **before** the existing
compiler DSL directory on the Lua module search path.

The four additional architectures are:

| Key | Target | Compiler-specific information |
| --- | --- | --- |
| `msp430` | Classic 16-bit MSP430, TI EABI | GR8/GR16 overlap, reserved PC/SP/SR/CG, conditional R4 frame pointer, normal/interrupt save sets, tied two-address operands, carry dependencies, constant-generator selection, SLEIGH p-code |
| `lanai` | 32-bit big-endian Lanai | Constant registers, reserved aliases, conditional R14 base pointer, stack/inreg/fast argument rules, spill-offset expansion, ALU/load-store resources and LLVM scheduling model |
| `xcore` | XS1 XCore | GRRegs allocation class, reserved CP/DP/SP/LR, conditional R10 frame pointer, argument/return registers, register scavenging, scaled short/long spill offsets, resource-I/O ordering |
| `arc` | ARCv2 EM/HS, ARC EABI | GPR32 r0-r63 + STATUS32, reserved SP/GP/FP/ILINK/BLINK, callee-saved r13-r25+gp+fp, args r0-r7/rets r0-r3, LIMM/compact immediates, STATUS32 .f/.cc dependencies, NoItineraries hazards |

The assembler subsets are intentionally representative; the four new
targets export **all target TableGen instruction records** for compiler
selection, including pseudos and custom-packed encodings. For existing
targets, instruction records cover every TableGen form matching a declared
assembly mnemonic. SPIR-V and DirectX export their available target
instructions independently of assembly spellings. `coverage` lists matched
and unmatched mnemonics. TableGen record IDs are separate from assembler
register names; use `AsmName`, `AltNames`, `SubRegs` and `SubRegIndices` to
map them, and apply ISA-mode/feature predicates.

### Schema (`backend.schema = 1`)

The extension has these fields:

* **`register_allocation`**: resolved records keyed by TableGen ID.
  Register classes retain `RegTypes`, `Size`, `Alignment`, `MemberList`,
  `AltOrders`, allocation priorities, copy costs and pressure-set properties.
  Registers retain encodings, DWARF numbers, aliases and subregister
  relations. Calling-convention actions, stack slots, shadow registers,
  and callee-saved sets are included when provided by TableGen.
* **`instructions`**: instruction IDs map to structured input/output
  operand DAGs, operand ties/early-clobbers in `Constraints`, implicit
  `Defs`/`Uses`, `Pattern` DAGs, feature predicates, commutation and
  rematerialization flags, memory/control effects, code size, `SchedRW`,
  itineraries, and `Inst` bit vectors. Bit-vector entries are ordered
  least-significant bit first; integers are fixed bits, `varbit` entries
  reference operand bits, and `null` bits are unresolved. A custom
  encoding cannot be reconstructed by replacing null bits with zero.
* **`instruction_selection`**: operand definitions, immediate predicates,
  feature predicates, complex address matchers, and applicable standalone
  selection patterns. DAGs preserve operators, arguments and operand names.
  C++ predicate/transform bodies are data for a consumer to implement;
  they are not executed by this DSL.
* **`instruction_scheduling`**: CPU models, issue width, buffer sizes,
  resources, write latency, resource occupancy, read advances, itinerary
  stages, scheduling aliases and instruction overrides. Model coverage is
  recorded in `coverage.selected_schedule_models`. The large X86, AArch64,
  ARM, AMDGPU, Hexagon, Mips, RISC-V and PowerPC banks select representative CPU models;
  other targets retain all available models. Referenced records are retained
  in the dependency closure.
* **`dependencies`**: transitive record references needed by the exported
  data, including anonymous calling-convention/scheduling rules, selection
  fragments, value types and DAG operators. A `{"kind":"def","def":"ID"}`
  reference resolves across all these record maps. Each record retains
  source locations and inherited TableGen class names.
* **`compiler_policy`** (new architectures, AArch64 and RISC-V): sourced ABI
  rules and LLVM C++ rules beyond TableGen, including reserved-register masks,
  conditional frame/base-pointer reservations, spill requirements,
  scavenging rules and scheduling hazards.
* **`sleigh`** (AArch64, AVR, RV32, RV64, MSP430, TriCore): DomSleigh
  constructors with p-code, token fields, address spaces, contexts,
  attachments and macros. Helper constructors are included because top-level
  semantics reference them. TriCore additionally includes its parsed compiler
  spec: argument/return locations, preserved registers and data layout.
* **`provenance` / `coverage`**: tool/source revision, source SHA-256s,
  upstream links, licensing, exported instruction scope and limitations.

**Allocator rules:** `MemberList` is a register-set expression, not a flat
allocation mask. Expand `add`/`sub`/`sequence`/tuple expressions, apply
reserved-register rules, and track overlapping subregisters. A class's
`isAllocatable` bit does not imply every member is allocatable in every
function. ABI-preserved subregisters may cover only part of a physical
register.

**Scheduler rules:** choose a processor model before resolving `SchedRW`,
`InstRW`, variants, resources and read advances. Latency and resource
occupancy are different quantities. Honor implicit register dependencies,
memory ordering, barriers, delay slots, convergent operations and calls.
`CompleteModel = 0` explicitly permits missing model entries. Negative
default timing values and JSON `null` mean unspecified/unresolved; they
are not zero-cost operations. LLVM's JSON dump can leave memory-effect
bits unresolved pending pattern inference. MSP430 and XCore do not provide
complete numerical timing models; their policies retain dependencies and
ordering constraints without inventing cycle counts. Virtual ISAs such as
SPIR-V/DirectX/Wasm do not define physical execution-port timings.

### Regeneration

Dependencies: Python 3, the installed `domsleigh`, Lua, `llvm-tblgen` and
`llvm-mc`. Use target **and include** sources matching the tool version.
The environment's unversioned architecture bank contains newer TableGen
syntax, so this snapshot uses LLVM **22.1.8**, commit
`ca7933e47d3a3451d81e72ac174dcb5aa28b59d1`, with LLVM 22.1.8's tool.

```sh
python stdspec/tools/extract_backend.py \
  --src /path/to/llvm-project/llvm/lib/Target \
  --include /path/to/llvm-project/llvm/include \
  --revision llvmorg-22.1.8 \
  --sleigh /path/to/Ghidra-CPU

python stdspec/tools/validate_backend.py \
  --dsl-dir /path/to/domsymas/compiler/stdspec
```

`--arch <key>` can be repeated to regenerate selected architectures.
`tools/backend_policies.json` contains the manually reviewed C++/ABI rules
and their web references. Web searches were supplemented by reading LLVM's
upstream calling-convention and register-info files directly. In particular,
Lanai C arguments are normally stack-passed, and XCore's frame pointer is
R10, not an extra architectural register. AAPCS64 and the RISC-V psABI were
also checked on the web: AArch64 v8-v15 preservation covers only the low
64 bits under the base PCS, and RISC-V gp/tp are unallocatable. Their
policies include conditional reservations and vector/partial-register ABI
constraints useful for allocation.

Validation executes every spec through the real DSL, parses the emitted
JSON, checks transitive record references and implicit registers, verifies
representative ties/reservations/scheduling facts, and compares new MSP430
and Lanai encoders against `llvm-mc`. XCore has no LLVM assembly parser;
its exposed short encoders are checked against resolved TableGen bit vectors
at register/immediate boundaries.

To emit a document, for example:

```sh
LUA_PATH='stdspec/?.lua;/path/to/domsymas/compiler/stdspec/?.lua;;' \
  lua stdspec/msp430.dsymspec > msp430.json
```

## C++ access (`Symspec.hpp`)

`../Symspec.hpp` is a header-only C++20 API for loading, parsing,
serializing and feeding these files to other apps. It links only
Boost.JSON (`-lboost_json`).

```cpp
#include "Symspec.hpp"
symspec::Repository repo("stdspec");
auto spec = repo.load("msp430").value(); // backend Lua, no Lua runtime
auto add = spec.record("ADD16rr").value(); // ties, Defs/Uses, Pattern, SchedRW
auto members = spec.register_members("GR16").value(); // allocation order
spec.save("msp430.json");
spec.feed([](std::string_view json) -> symspec::Status {
    return forward_to_app(json);
});
```

It parses emitted JSON, generated `backend/*.lua` modules and the original
`.dsymspec` DSL (via the existing Lua runtime or a host `Evaluator`),
validates schema/references, runs backend queries (record lookup,
dependency closure, register-set expansion, reserved registers, CPU models),
edits/copies specs, emits JSON/Lua/NDJSON, integrates with `dsl::Serializer`
via `symspec::Codec`, and discovers architectures lazily through
`symspec::Repository`. See `../tests/symspec_test.cc` for roundtrips,
bank validation and app-integration coverage.
