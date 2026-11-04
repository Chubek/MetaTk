# Chapter 26 — CHIP-8 state and instructions

[Previous](25-adding-a-machine.md) · [Contents](README.md) · [Next](27-chip8-profiles-and-scheduling.md)

## Public machine model

`metavm::machines::Chip8`, declared in `VMLibrary/CHIP-8/Chip8.hpp`, directly
interprets selected classic CHIP-8 opcodes. It is not a Machinery module and
does not require a C host-import table.

| Member | Representation |
|---|---|
| `memory` | 4096 bytes; font starts at `0x50` |
| `v` | Sixteen byte registers; `v[15]` is VF |
| `stack` / `sp` | Sixteen return addresses and byte stack pointer |
| `pc` / `i` | Program counter and address register |
| `keys` | Sixteen sampled boolean keys |
| `display` | Row-major 64×32 bytes containing monochrome bits |
| `delay` / `sound` | Byte timers, decremented by host ticks |
| `random_state` | Deterministic xorshift PRNG state |
| `quirks` | Selected opcode/input semantics |

`load(rom)` accepts up to 3584 bytes at `0x200`. Success resets the machine,
font, timers, display, PRNG, and pending waits while preserving quirks.
Oversized input returns false without changing state. An empty ROM is not a
meaningful executable program merely because it fits; fetched zero bytes are
not treated as a universal halt.

## Implemented classic instruction families

| Encoding | Effect |
|---|---|
| `00E0`, `00EE` | Clear display, return |
| `1nnn`, `2nnn` | Jump, call |
| `3xkk`, `4xkk`, `5xy0`, `9xy0` | Conditional skips |
| `6xkk`, `7xkk`, `8xy0` | Load, immediate addition, register move |
| `8xy1/2/3` | OR/AND/XOR with profile-specific VF handling |
| `8xy4/5/7` | Add and subtract variants with carry/no-borrow VF |
| `8xy6/E` | Shifts with profile-selected source and VF bit |
| `Annn`, `Bnnn` | Set I, profile-selected indexed jump |
| `Cxkk` | PRNG byte masked by `kk` |
| `Dxyn` with n > 0 | XOR sprite, collision VF, clip/wrap policy |
| `Ex9E`, `ExA1` | Skip based on sampled key state |
| `Fx07/15/18` | Read/set delay, set sound timer |
| `Fx0A` | Host-sampled key wait |
| `Fx1E`, `Fx29` | Add VX to I, select low-nibble font glyph |
| `Fx33`, `Fx55`, `Fx65` | BCD store, register store/load |

Malformed low nibbles and unsupported encodings return `invalid_opcode`.
`0nnn` machine-code calls and zero-height SCHIP drawing are unsupported.
No SCHIP/XO-CHIP extensions are inferred from a ROM name.

## Execution and errors

`step()` executes at most one instruction and returns `Status`: `running`,
`waiting`, `waiting_vblank`, `invalid_opcode`, `bounds`, `stack_overflow`, or
`stack_underflow`. `running` means completion, not termination. There is no
classic-ROM general-purpose halt API; the host supplies an attempt budget.

Memory spans are checked before BCD/register transfers or sprite reads.
`Fx1E` and configured post-transfer I updates wrap to 12 bits. Arithmetic snapshots
both register operands before writing VF, including cases where VF is itself
an operand. General direct edits of the public state remain the host's
responsibility; public-state corruption does not define a valid guest machine.

```cpp
metavm::machines::Chip8 machine;
if (!machine.load({0x60, 42})) throw std::runtime_error("ROM too large");
auto status = machine.step();
if (status != metavm::machines::Chip8::Status::running || machine.v[0] != 42)
    throw std::runtime_error("CHIP-8 result mismatch");
```

**References:** [implementation](../VMLibrary/CHIP-8/Chip8.cpp), [profile contract](../docs/CHIP8.md).
