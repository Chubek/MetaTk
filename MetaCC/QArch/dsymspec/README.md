# DomSymAsCC stdspec

## MetaChain executable consumers

`amd64.dsymspec.yaml` now also owns the executable `asmtk` encoding extension
and audited `metachain` value-backend profile. Run
`python3 -B Arch/dsymspec/tools/generate_asmtk.py` and
`python3 -B Arch/dsymspec/tools/generate_backend.py` from the repository root
after editing them; add `--check` for freshness validation. Outputs are checked
in under `generated/`, not regenerated implicitly during ordinary C++ builds.
Generation requires Python, PyYAML and jsonschema, not Lua.

The assembler generator derives shared bytes, ModRM orientation/groups and
fixups from the existing encoder bodies and resolves explicit additional
width/memory forms and condition aliases from the extension. Unknown encoder
mechanisms fail instead of evaluating arbitrary pseudo-code. Normal YAML-to-JSON
conversion preserves the resolved extension. The backend generator checks
actual instruction patterns/classes/ties/predicates/effects, resolves exact
processor-specific scheduling costs and derives allocation order from GR64.
The YAML declares the SysV ABI policy and explicitly asserts pure BV64 value
semantics. These are not claims that EFLAGS or memory effects can be discarded
from arbitrary machine instructions.

`metachain/TargetBackend.hpp` supplies selection, relaxed scheduling,
register-family/palette allocation, ordered/e-graph value rewrites and real
bounded SysV leaf-function emission. The [architecture backend guide](../../manual/architecture-backends.md)
defines its APIs, safety checks, tests and unsupported features. Metadata for
the other architectures is accessible through backend JSON; it does not
automatically become executable assembler support. The existing RV64 MLVM
generator remains a separate consumer of `riscv64.dsymspec.yaml`.

`symspec::Repository` discovers `.dsymspec.yaml` files and loads `backend/*.json`
directly. Source loading consumes converted JSON, including the checked-in
AMD64 bank; it never silently executes YAML or starts a Lua conversion process.

`.dsymspec.yaml` files are architecture specifications for DomSymAsCC. Each file
describes one architecture's registers, features, relocations, sections and a
representative instruction set. Converters turn a spec into a JSON document, a
Lua data module or binary bytecode. The JSON document carries a structured
`backend` extension for register allocation, instruction selection and
scheduling.

The specs used to be Lua scripts (`.dsymspec`) run by the DomMune interpreter.
They are now YAML, with the encoder code written as imperative pseudo-code, so
that nothing needs a Lua runtime to read them.

```text
  <arch>.dsymspec.yaml ──validate.py──▶ checked spec
          │
          ├── dsymspec2json.py  ──▶ build/json/<arch>.json       JSON document (domsymas input)
          ├── dsymspec2lua.py   ──▶ build/lua/<arch>.lua         Lua data module
          └── dsymspec-cc.py    ──▶ build/bytecode/<arch>.dsymc  binary bytecode
          ▲
  backend/<arch>.json  (generated backend extension, referenced by the spec)
```

## Layout

```text
schema/dsymspec.schema.json    JSON Schema (draft 2020-12) for the YAML format
<arch>.dsymspec.yaml           one specification per architecture (28 files)
backend/<arch>.json            generated backend extension (JSON)
backend/NOTICE.md              licensing of the generated data
tools/validate.py              schema and semantic checks
tools/dsymspec2json.py         YAML -> JSON document
tools/dsymspec2lua.py          YAML -> Lua data module
tools/dsymspec-cc.py           YAML -> binary bytecode; --decode for the reverse
tools/dsymspec_core.py         shared loader, pseudo-code parser and document builder
tools/extract_backend.py       legacy generator for backend data (see Regeneration)
tools/validate_backend.py      legacy checker for backend data (see Regeneration)
tools/backend_policies.json    reviewed compiler policies used by extract_backend.py
```

Requirements: Python 3 with `pyyaml` and `jsonschema` (`pip install pyyaml jsonschema`).
The tools were tested with Python 3.13. Nothing else is needed to validate or convert.

### MLVM's bounded RV64 consumer

`tools/generate_mlvm.py` validates `riscv64.dsymspec.yaml` and its backend ABI
policy, then generates `MLVM/generated/arch/riscv64.qs`. Run it after changing
the RV64 instructions, operand bounds or allocation policy; `--check` checks
freshness without writing. MLVM consumes the emitted QScript, not a parallel
handwritten opcode or register table. Only audited single-word `EMIT32LE`
encoders are supported; new effects, missing RV64IM features and unsafe leaf
register allocation are rejected. It does not replace the general converters
or add RV64 ELF/linker support. See [MLVM's target contract](../../manual/mlvm.md)
and `tests/mlvm_arch.py` for regeneration and fail-closed tests.

## Writing a spec

A spec is a YAML mapping. Every key is optional except `schema` and `targets`.
Records reference one another by **name**; the tools check that every reference
resolves.

```yaml
schema: 1
targets:
  - {arch: lanai, addr_size: 32, op_size: 32, addr_def: 32, align: 4,
     byte_order: big, obj_format: ELF32, abi: Lanai32, calling_conv: lanai32,
     features: [base]}
features:
  - {name: base, desc: "Lanai 32-bit integer ISA"}
registers:
  - {name: r0, class: gpr, width: 32, enc: 0, flags: [readable]}
mnemonics:
  - {name: add}
encoders:
  - name: add_i_lo
    max_bytes: 4
    body: |
      REQUIRE imm >= 0
      REQUIRE imm <= 65535
      EMIT32BE rd*8388608 + rs*262144 + imm
forms:
  - mnemonic: add
    encoder: add_i_lo
    operands:
      - {kind: reg, class: gpr, width: 32, access: read}
      - {kind: imm, width: 16, imm_min: 0, imm_max: 65535, signed: false}
      - {kind: reg, class: gpr, width: 32, access: write}
backend:
  file: backend/lanai.json
```

Top-level keys, in the order the JSON document uses them:

| Key | Holds | Notes |
| --- | --- | --- |
| `schema` | `1` | Document schema version. |
| `targets` | target records | At least one. `align` is a power of two. |
| `features` | feature records | Names are used by `targets.features` and by forms. |
| `registers` | register records | `parent` names another register. |
| `mnemonics` | mnemonic records | |
| `encoders` | encoder records | `body` is pseudo-code (below). |
| `forms` | instruction forms | `mnemonic`, `encoder` and `syntaxes` must resolve. |
| `syntaxes` | assembly syntaxes | |
| `sections` | section records | |
| `linkages` | symbol linkage records | |
| `relocs` | relocation records | |
| `debug` | debug notes | |
| `backend` | `{file: backend/<arch>.json}` | Optional. |

Field names and value sets are the same as in the JSON document below, with
two differences: an encoder has `body` instead of `source`, and a form may omit
`ops`, which defaults to the number of operands. The schema lists every allowed
field and value. Assembler spellings such as `$r0`, `%f0` or `16-bit` are kept
verbatim as names.

The converters add only the default that the JSON format documents: a form's
`ops` becomes the number of operands. Other optional fields are not filled in. The
examples in the JSON section show `flags` on features and linkages, and `offset`,
`overflow` and `addend` on relocations. Those values were filled in by the Lua DSL's
`emit()`, which is not in this archive. If the consumer needs them, write them in
the YAML.

Comments (`#`) are allowed anywhere in YAML. The migrated specs keep the
explanatory comments from the original Lua files.

## Encoder bodies: imperative pseudo-code

An encoder body is a block of statements, one per line, executed top to bottom.
Each statement is an upper-case verb followed by its arguments, separated by
commas:

| Statement | Effect | Algol-like form (JSON `source`, Lua) |
| --- | --- | --- |
| `REQUIRE e` | Fail the match unless `e` holds | `require(e);` |
| `EMIT8 e` | Emit one byte | `emit8(e);` |
| `EMIT16LE e`, `EMIT16BE e` | Emit a 16-bit value | `emit16le(e);`, `emit16be(e);` |
| `EMIT32LE e`, `EMIT32BE e` | Emit a 32-bit value | `emit32le(e);`, `emit32be(e);` |
| `EMIT64LE e`, `EMIT64BE e` | Emit a 64-bit value | `emit64le(e);`, `emit64be(e);` |
| `EMIT_IMM w, e` | Emit an immediate of `w` bits | `emit_imm(w, e);` |
| `EMIT_DISP w, e` | Emit a displacement of `w` bits | `emit_disp(w, e);` |
| `EMIT_MODRM a, b` | Emit a ModR/M byte | `emit_modrm(a, b);` |
| `EMIT_SIB a, b, c` | Emit a SIB byte | `emit_sib(a, b, c);` |
| `EMIT_RELOCATION s, T` | Attach relocation type `T` to symbol `s` | `emit_relocation(s, T);` |
| `FAIL "message"` | Reject the encoding | `fail("message");` |

Rules:

- One statement per line. Blank lines are not allowed.
- Arguments are expressions. An expression is built from identifiers (operand
  names such as `rd`, `imm`), decimal or `0x` hexadecimal literals, parentheses,
  and the operators `+ - * / % & | ^ ~ ! << >> < > <= >= == != && ||`.
  Expression text is copied into the Algol-like form unchanged.
- `FAIL` takes one double-quoted string.
- There is no control flow. An `if`/`else` cannot be written, and the current
  specs do not need one.

Example, from `riscv64.dsymspec.yaml`:

```text
EMIT32LE 0x00000033 + rd*128 + rs1*32768 + rs2*1048576
```

is written to the JSON document as `emit32le(0x00000033 + rd*128 + rs1*32768 + rs2*1048576);`.
The pseudo-code and the Algol-like form are interchangeable: the converters
translate one into the other with no change to the expressions.

## Converters

All tools take spec paths. With no paths they process every `*.dsymspec.yaml`
in the repository root. Each spec is validated before conversion, and nothing is
written for a spec that fails.

```sh
python3 tools/validate.py                          # check all specs (exit 1 on any error)
python3 tools/dsymspec2json.py                     # -> build/json/<arch>.json
python3 tools/dsymspec2json.py --stdout x86.dsymspec.yaml --compact
python3 tools/dsymspec2lua.py                      # -> build/lua/<arch>.lua
python3 tools/dsymspec-cc.py                       # -> build/bytecode/<arch>.dsymc
python3 tools/dsymspec-cc.py --decode build/bytecode/lanai.dsymc
```

`dsymspec2json.py` writes the JSON document described under **JSON document
schema**, including the `backend` member. The output is indented by default.
`--compact` removes the indentation.

`dsymspec2lua.py` writes a Lua module that returns one table with the same
fields. Encoder records carry the Algol-like `source` string that domsymas
reads. The backend is too large to write as a table, so it is kept as JSON text
in `backend_json`; decode it with a JSON parser. `--no-backend` leaves it out.

`dsymspec-cc.py` writes binary bytecode. The format is a checksummed, flat
stream: a header, a string table, and an opcode stream, with one opcode per
record kind and one per encoder statement. Expressions are stored as interned
text. The backend JSON is zlib-compressed. Every file is decoded again while it
is written, and compilation fails unless the result is identical to the JSON
document, so a bad encoding is not written. `--decode` prints the JSON document
of a `.dsymc` file.

Bytecode container (integers are little-endian; `uvarint` is unsigned LEB128):

```text
header   20 bytes  "DSYC", u16 version (1 or 2), u16 flags (0),
                   u32 string count, u32 string-table bytes, u32 code bytes
strings  count x (uvarint length, UTF-8 bytes)
code     opcode stream, ending with HALT (0x00)
trailer  32 bytes  SHA-256 of everything before it
```

| Opcode | Operand |
| --- | --- |
| `0x01 SOURCE_SHA256` | 32 raw bytes: SHA-256 of the YAML file |
| `0x10`–`0x19` | one record: `TARGET`, `FEATURE`, `REGISTER`, `MNEMONIC`, `FORM`, `SYNTAX`, `SECTION`, `LINKAGE`, `RELOC`, `DEBUG` (a tagged value) |
| `0x20 ENCODER` … `0x21 END_ENCODER` | encoder metadata (a tagged value), then its statements |
| `0x30`–`0x3D` | one statement each, `REQUIRE` … `FAIL`; operands are string indices |
| `0x40 BACKEND` | uvarint length, then zlib-compressed backend JSON |
| `0x41 EXTENSIONS` | map of resolved `asmtk` / `metachain` data; version 2 only |

Documents without the executable extensions still emit version 1 unchanged.
Documents carrying them emit version 2; the decoder accepts both, rejects
duplicate/unknown extension envelopes and never silently drops the extensions.
Both versions retain the source checksum and JSON round-trip self-check.

Tagged values: `0` null, `1` false, `2` true, `3` integer (zigzag varint),
`4` string (index), `5` list (count, values), `6` map (count, then key index
and value pairs).

## Migration from the Lua specs

Each original `.dsymspec` was executed once, under a shim that recorded every
call to the DSL (`target`, `feature`, `register`, `mnemonic`, `encoder`, `form`,
`syntax`, `section`, `linkage`, `reloc`, `debug`). The YAML was generated from
those recorded calls. Loops in the Lua were expanded into individual records,
so the YAML has every register and encoder written out. Comments were attached
to the record that follows them. Lua concatenations inside encoder sources were
evaluated first.

The encoder sources were converted to pseudo-code. The conversion is exact: the
original sources all use the statement form `stmt; stmt;`, and turning the
pseudo-code back gives the original text.

The `dsl` and `backend` Lua modules are no longer needed. The `backend/*.lua`
wrappers became `backend/*.json`.

## Legacy tools

`tools/extract_backend.py` regenerates the backend data from LLVM TableGen and
Ghidra SLEIGH. It still writes `backend/<arch>.lua` and reads specs through the
Lua DSL. `tools/validate_backend.py` runs the specs through the Lua DSL. Both
must be ported to YAML and JSON before they are used again.

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

## Compiler backend extension

Every spec references its backend data with `backend: { file: backend/<arch>.json }`.
The file is plain JSON (the content of the former `backend/<arch>.lua` modules,
unwrapped byte for byte). `dsymspec2json.py` embeds it as the `backend` member of
the document; `dsymspec-cc.py` stores it zlib-compressed; `dsymspec2lua.py` keeps it
as JSON text in `backend_json`. This preserves JSON `null`, empty arrays and
structured TableGen DAGs without requiring a Lua runtime.

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

> **Legacy.** `tools/extract_backend.py` and `tools/validate_backend.py` predate
> the YAML format. They still write `backend/<arch>.lua` and read specs through
> the Lua DSL, so they must be ported before they are used again. The steps
> below describe their intended use.


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
python3 tools/dsymspec2json.py lanai.dsymspec.yaml      # -> build/json/lanai.json
```

## Consuming the output from C++

`Symspec.hpp` is a header in the parent project, outside this directory. It was
written for the Lua specs and for the JSON document. Its loader for `.dsymspec`
files needs updating to read `.dsymspec.yaml` (or the JSON and bytecode outputs
of this directory's converters).
