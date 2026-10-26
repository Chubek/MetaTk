# Webber example assembler plugin

Webber is **a demonstrative GPPX plugin**, not an existing processor or Android/ELF assembler. Its bytecode is for a hypothetical stack VM and is not executable on an actual CPU.

## Enable

```sh
cmake -S . -B build -DGPPX_BUILD_WEBBER=ON
cmake --build build
./build/gppx --plugin ./build/plugins/gppx_webber.so examples/webber/demo.gppx
./build/gppx --plugin ./build/plugins/gppx_webber.so -e '#embed "examples/webber/extended.wbr" wbr_hex'
```

## Assembly language

One instruction or data directive per line. Semicolons start comments except inside quoted strings. Labels (`entry:`) identify byte offsets; forward jumps are resolved on the second pass. `.EQU NAME, VALUE` declares an integer constant. Symbol operands accept `name+offset` or `name-offset`.

| Instruction | Opcode | Encoding |
|---|---|---|
| NOP | 00 | No operand |
| PUSH | 10 | Signed/unsigned 8-bit immediate |
| DUP, DROP, SWAP | 11, 12, 13 | No operand |
| ADD, SUB, MUL, DIV, MOD | 20–24 | No operand |
| PRINT | 30 | No operand |
| JMP, JZ, JNZ | 40, 41, 42 | Absolute little-endian 16-bit target |
| HALT | FF | No operand |

Data directives: `.BYTE A,B`, `.WORD A,B` (little endian), `.ASCII "..."`, `.ASCIZ "..."` (NUL terminated), `.SPACE n` (zero fill), `.ALIGN n` (power of two up to 4096), `.ORG absolute_address` (zero-filled forward seek). Quoted strings recognize `\\n`, `\\r`, `\\t`, `\\0`, `\\xHH`, `\\"` and `\\\\`.

The assembler rejects duplicate labels, undefined symbols, malformed operands, backward `.ORG`, invalid alignment, bad escapes and programs larger than 65,536 bytes. `Assembler.hpp` exposes `webber::assemble()` and `asCArray()` for C++ code.

## GPPX additions

`#wbr_upper text` and `#wbr_lower text` transform directive arguments. `WBR_UPPER(text)` and `WBR_VERSION()` are callable macros. `#embed "file.wbr" wbr_asm` produces C-style byte literals, while `wbr_hex` concatenates byte values as hex. The `wbr_contains` pattern matcher provides substring tests in `#ifmatch`.

The reference assembler emits bytecode only: it does not execute the VM or link separate object files. See `plugins/webber/CATALOGUE.yaml` for the capability registry, and `examples/webber/extended.wbr` for the extended syntax test.
