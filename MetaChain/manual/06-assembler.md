# 6. ASMtk assembler

## Scope and output modes

ASMtk contains a real x86-64 encoder. It accepts Intel-style destination-first
syntax, resolves forward labels, emits data/string/alignment directives, and
produces raw bytes or ELF64 relocatable objects. The instruction subset below
is the supported contract; unknown mnemonics and unsupported operand forms are
errors, even if optional Symspec metadata describes such a form.

```cpp
asmtk::Assembler assembler;
auto raw = assembler.assemble("mov rax, 60\nmov rdi, 42\nsyscall\n");
auto object = assembler.assemble_object(
    ".global _start\n_start: mov rax, 60\nmov rdi, 42\nsyscall\n");
```

`AssemblyResult::bytes` is the raw image in the first call and the entire ELF
object in the second. `symbols` contains source label offsets within the text
section. `relocations` records symbolic fixups and their source locations. Test
`success()` or the boolean conversion before using the bytes. Any failed
assembly clears its byte artifact; partial instruction bytes are not returned
as apparently usable output.

## Lexical rules

Mnemonics, directive names, and register spellings are case-insensitive. Labels
are case-sensitive. A label starts with an ASCII letter, underscore, period, or
dollar sign and continues with those characters or digits. Label definitions
end in `:`. Labels and statements may share a line; multiple labels may precede
a statement. Duplicate labels are errors. Label names are limited to 4096 bytes.

`;` and `#` start comments outside quoted strings. Commas in strings are not
operand separators; comment characters and colons in strings are not stripped.
One statement is written per line. No macro processor, include directive,
conditional assembly, AT&T syntax, or numeric local-label convention is implied.

## Registers and operand forms

* 64-bit: `rax rcx rdx rbx rsp rbp rsi rdi r8` through `r15`.
* 32-bit: `eax ecx edx ebx esp ebp esi edi r8d` through `r15d`.
* 8-bit: `al cl dl bl spl bpl sil dil r8b` through `r15b`, for `mov` only.

High-byte registers `ah`, `ch`, `dh`, and `bh`, 16-bit registers, vector
registers, and segment overrides are not supported. Register-register
instructions require equal operand widths. Byte-register moves generate the
required REX prefix for the low-byte forms that need it.

Memory forms for `mov` and `lea` are:

```text
[base]
[base+signed-displacement]
[base-signed-displacement]
[rel symbol]
[rel symbol+signed-addend]
```

The base is a 64-bit register. Displacements fit signed 32 bits. There is no
scaled index operand. Base-address forms deliberately use a disp32 encoding,
including a zero displacement, so sizes are deterministic. `rsp`/`r12` receive
the required SIB byte; `rbp`/`r13` are not accidentally encoded as RIP-relative.
RIP-relative operands use an explicit `rel` keyword.

Memory width follows the other register operand. There are no ambiguous
memory-immediate operations requiring an implicit width. `lea` requires a 32-
or 64-bit destination. Raw RIP-relative fixups are independent of `origin`.

## Implemented instruction forms

| Instruction | Operands |
|---|---|
| `nop ret syscall int3 ud2 cqo leave` | none |
| `mov` | register/register, register/immediate, register/symbol, register/memory, memory/register |
| `lea` | 32/64-bit register, memory |
| `add sub and or xor cmp` | 32/64-bit register, matching register or signed imm32 |
| `test` | two matching 32/64-bit registers |
| `imul` | two matching 32/64-bit registers |
| `inc dec neg not div idiv` | one 32/64-bit register |
| `shl shr sar` | 32/64-bit register, `cl` or in-range integer literal |
| `push pop` | one 64-bit register |
| `jmp call` | symbol expression or one 64-bit register |
| conditional jumps | symbol expression |

Conditional mnemonics: `jo jno jb jc jae jnc je jz jne jnz jbe ja js jns jp jnp
jl jge jle jg`. Conditional and direct unconditional branches always use rel32;
there is no short-branch relaxation. Indirect register branches use their
architectural `FF` group encoding.

`mov r64, immediate` always uses a full 64-bit immediate. Arithmetic immediates
always use signed imm32. This favors predictable layouts over minimum code
size. Native MetaIL lowering relies on these supported forms, not on external
assembly tools.

## Directives

| Directives/aliases | Effect |
|---|---|
| `.byte`, `db` | one-byte integer fields |
| `.word`, `.short`, `dw` | two-byte integer fields |
| `.long`, `.int`, `dd` | four-byte integer/symbol fields |
| `.quad`, `.8byte`, `dq` | eight-byte integer/symbol fields |
| `.ascii` | concatenate decoded strings |
| `.asciz`, `.string` | decoded strings, each followed by NUL |
| `.zero`, `.space`, `.skip` | one literal count of zero bytes |
| `.align`, `.balign` | pad to a positive power-of-two byte boundary |
| `.text` | select the sole emitted text section |
| `.global`, `.globl` | export comma-separated defined symbols |
| `.extern` | declare comma-separated unresolved object symbols |

Integer literals use decimal or `0x/0b/0o` prefixes. Positive fields may use all
bits; negative fields must fit the signed range of their width. Thus `.byte
255` and `.byte -128` succeed, while `.byte 256` and `.byte -129` fail rather
than truncating silently. Eight-byte positive values may reach `UINT64_MAX`.

Strings accept single/double quotes and `\n`, `\r`, `\t`, `\0`, escaped quotes,
escaped backslashes, and exactly two hexadecimal digits after `\x`. A trailing
escape, missing quote, or unknown escape is diagnosed. The output bound includes
the terminating NUL for `.asciz`; a one-byte output limit cannot admit
`.asciz "x"`.

This assembler emits a single executable `.text` section. `.data`, `.bss`, and
arbitrary `.section` directives are not silently mapped into it. If writable
sections are needed, use an external producer of supported ELF objects or add
an explicitly tested multi-section assembler backend.

## Symbol expressions and relocations

Supported expressions are `symbol`, `symbol+integer`, and `symbol-integer`.
They are not a general arithmetic expression evaluator. Raw output resolves
all referenced symbols locally and applies `Options::origin` for absolute
addresses. Raw unresolved symbols fail even if `.extern` is present.

ELF output retains symbolic fixups for the linker, including local label
references. References without a local definition require `.extern`. Exported
symbols must have local definitions. Local symbols precede globals in the ELF
symbol table. The object includes `.rela.text`, `.symtab`, `.strtab`,
`.shstrtab`, and a non-executable-stack marker.

Relocation kinds are `R_X86_64_64`, `R_X86_64_32`, `R_X86_64_PC32`, and
`R_X86_64_PLT32` for calls. The PC-relative addend includes `-4` because ELF's
relocation place is the start of the rel32 field, whereas the CPU bases a
relative displacement on the following instruction address.

## Options and metadata

`little_endian_data` defaults true and controls integer data directives only;
it does not alter x86 instruction byte order. `allow_unknown_directives`
defaults false. Its true value is an explicitly permissive compatibility mode
and should not be used for validation of untrusted or unfamiliar assembly.

`max_output_bytes` and `max_source_bytes` default to 64 MiB each. ELF metadata
counts toward the output limit, so a raw text image that exactly fills that
limit cannot necessarily be wrapped in an object at the same limit.

With `METACHAIN_ENABLE_SYMSPEC`, `Architecture` retains an immutable specification
and exposes borrowed query results. The built-in target remains x86-64;
metadata discovery is not a substitute for an instruction encoder.
