# ArchAS / `domas`

GAS-compatible assembly and MetaArchIR (`.mair`) compilation through MetaArch and
Dsymspec, with Lua templates and C/C++ shared-library directive plugins.

```sh
cmake -S . -B build-archas -DARCHDB_BUILD_ARCHAS=ON
cmake --build build-archas --parallel 4
build-archas/domas -T amd64 --intel-syntax ArchAS/examples/answer.mair
```

The default output is a relocatable object beside the input (`answer.o`).
`--emit asm` writes expanded GAS; `--emit bin` or `hex` produces flat bytes when
there are no relocations and at most one nonempty allocated section.
`--backend builtin --emit hex` runs MetaArch's encoders directly. `--emit json`
produces a MetaArchIR section image including relocation metadata.

See `domas --help` for the full option set. A native backend uses GNU `as` on
x86 hosts for x86 targets, otherwise `llvm-mc`. Select one explicitly using
`--backend gas|llvm`; `--assembler PATH` and repeated `--assembler-arg ARG`
configure cross assemblers. Processes are invoked with argument arrays.

## Language and extensions

Native instructions, symbols, numeric local labels, expressions, sections,
visibility, data, `.type`, `.size` and other GAS directives go to the selected
native assembler. Intel syntax is supported on x86/amd64; AT&T is the default.
MetaArchIR's `encode`, `form`, `section`, `global`, `align`, `ascii`, and fixed-width
`byte/word/long/quad` spellings are accepted alongside GAS directives.
`encode` and `form` use the selected database's VM and explicit named bindings.

```asm
section .text
global decrement
decrement:
form 11, rd=x10, rs1=x10, imm=-1
encode ret
```

Compile this for `riscv64`. Operand/form meanings come from the database;
retargeting selects another specification, rather than translating native
instructions between ISAs. GAS instructions use the native assembler's encoder.

The frontend expands `.macro/.endm`, defaults, `:req`, `:vararg`, keyword arguments,
`\parameter`, `\@`, `\()`, `.exitm`, `.purgem`, `.rept`, `.irp`, `.irpc`,
`.if/.elseif/.else/.endif`, `.ifdef/.ifndef`, GAS numeric/string conditionals
(`.ifeq/.ifne/.iflt/.ifle/.ifgt/.ifge`, `.ifc/.ifnc/.ifeqs/.ifnes/.ifb/.ifnb`),
and `.include`. Omitted macro arguments select defaults. Preprocessor expressions
are checked signed 64-bit absolute expressions with arithmetic, shifts, bitwise,
comparison and boolean operators. Boolean operands are both evaluated. Absolute
`.set`, `.equ`, `.equiv` definitions are available to the preprocessor;
section-relative aliases are passed to the native assembler. Includes resolve relative to the
including file, then `-I` directories. Include cycles and expansion recursion fail
with positioned diagnostics. Alternate macro mode is not implemented.

Comments: `/* ... */`, trailing `//`, and GAS `#` comments; ARM/AArch64 keep
non-leading `#` for immediates. Quotes protect comment markers and semicolons.
Semicolons separate statements. Source locations survive includes and templates;
macro-generated diagnostics use the invocation location.

### Lua

```asm
{% local count = domas.defines.COUNT or 4 %}
{% for i=1,count do %}
.4byte {{ i*i }}
{% end %}
```

`{% code %}` and `{{ expression }}` form one Lua chunk per input/include;
control flow can span tags. `{# comment #}` is a template comment. Templates run
before GAS preprocessing, including templates inside macro definitions or inactive
`.if` blocks. Use Lua conditionals when execution itself must be conditional.
Template delimiters inside Lua strings/comments/long strings are respected.

Load helpers with `--lua FILE` (repeatable), or define them in a template tag:

```lua
domas.directive("u32", function(arguments, context)
    return ".4byte " .. arguments
end)
```

Use `.u32 42` in source. The callback receives raw argument text and a context
table (`file`, `line`, `column`, `target`, `syntax`). Return source or `nil`;
`domas.emit(text)` also emits source, adding a terminating newline. Returned source
is recursively macro/directive-expanded. `domas.target`, `target_index`, `syntax`,
`defines`, and `domas.register(name)` expose the selected MetaArch target.
The register query returns `name`, `enc`, `width`, `class`, or nil.

Lua runs with its standard libraries. Scripts and plugins are trusted build
code. Each compilation has a fresh VM, with allocation/instruction limits.
Lua support is optional: `ARCHAS_LUA=OFF`. CMake prefers system Lua >=5.3,
then the workspace source under `third_party`, then FetchContent. Set
`ARCHAS_LUA_DIR` to a Lua source directory to explicitly use that checkout.

### Native plugins

`ArchASPlugin.h` defines C11 ABI v1. Export `archas_plugin_v1()`, returning a
versioned `ArchASPlugin` descriptor. `init` registers named callbacks through
`ArchASHost::register_directive`; each callback emits source through its supplied
`ArchASEmitter`. `shutdown` releases plugin state before `dlclose`.

```sh
cc -std=c11 -fPIC -shared -IArchAS ArchAS/examples/directives.c -o pair.so
domas -T amd64 --plugin ./pair.so --emit asm input.mair
```

The example defines `.pair EXPRESSION`. No link to ArchAS is required. Names are
copied, invocation pointers are borrowed, callback results use 0 for success,
and failures use `error()` plus a nonzero return. No C++ exceptions may cross the
ABI. Duplicate/built-in names and incompatible ABI versions are rejected.
Plugins load before Lua helpers, then source templates execute. Shared-library
loading is POSIX-only and optional (`ARCHAS_PLUGINS=OFF`).

## Library

Link `MetaArch::as` and include `ArchAS/ArchAS.hpp`:

```cpp
archas::Options options;
options.backend = archas::Backend::builtin;
options.format = archas::Format::hex;
archas::Assembler as(archas::load_target("amd64"), options);
auto result = as.compile("encode ret\n", "example.mair"); // c3\n
```

`Assembler::directive` registers a C++ callback returning source. Every `compile`
call creates an independent translation unit. Target handles share immutable
specifications; outputs own bytes, diagnostics and dependency paths. Const calls
may run concurrently when supplied callbacks/plugins are thread-safe. Mutating an
Assembler requires caller synchronization. Semantic errors throw `archas::Error`
with a `Location`; I/O/tool failures throw `IOError`; allocation may throw
`std::bad_alloc`. The existing C/MetaArchIR APIs are unchanged.

## Output contracts

- Target lookup: explicit `--spec`, explicit `--db`, `METAARCHDB`, current/parent
  `MetaArchDB`, configured install data, source data. `--db ArchDB` loads YAML
  Dsymspec files directly. `--target-index` selects an exact target record.
- Aliases: `x86_64`, `arm64`, `i386`, `rv32`, `rv64`. Native triples are declared in
  `src/Targets.def`; unsupported native targets require explicit tool configuration.
- `--emit asm` lowers `.encode/.form` to bytes; it does not validate remaining
  native instruction operands. Object/bin/hex modes invoke the native assembler.
- Builtin bin/hex/json supports explicit encoders/forms, labels, globals, sections,
  fixed-width data, strings, space and power-of-two byte alignment. It rejects
  native instructions and unsupported directives. JSON follows MetaArchIR image
  schema 1; relocation-bearing/multisection images cannot be flattened.
- Encoded relocations lower to ELF64 `.reloc` on amd64 after cross-checking
  width, offset, byte order and PC-relative metadata against ArchBFD. Addends are
  explicit, e.g. `encode call, sym=@callee-4`. Other encoded relocations use JSON;
  native instructions retain the native assembler's full relocation support.
  GNU `.reloc` does not support REL in-place addends, so those are not guessed.
- Native flat output uses the existing ArchBFD object reader, rejects relocations
  and multiple nonempty allocated payload sections, and retains zero-filled
  NOBITS data. ELF NOTE metadata (including synthesized GNU properties) is omitted.
- `--check` compiles the selected output mode without writing. File output is
  atomically replaced after successful compilation on POSIX. The CLI rejects
  input/dependency aliases, including symlinks/hardlinks. `-` supports pipelines.
- Status: 0 success; 1 invalid source/target; 2 usage; 3 I/O or unavailable tool.

## Verification

```sh
ctest --test-dir build-archas --output-on-failure -R archas
```

Unit coverage exercises independent multi-ISA byte anchors, checked forms,
macros, conditions, Lua, callbacks, diagnostics and resource limits. CLI tests
exercise real ELF objects, cross-target LLVM assembly, relocations, a C plugin,
and linking/executing with the system compiler and ArchLD when available.
