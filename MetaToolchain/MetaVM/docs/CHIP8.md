# Classic CHIP-8 profiles

`VMLibrary/CHIP-8/Chip8.hpp` is a direct, bounded classic-opcode interpreter:
4 KiB memory, sixteen byte registers, sixteen return addresses, 64×32 monochrome
pixels and sixteen sampled keys. `load` accepts raw program bytes at `0x200`
(maximum 3584 bytes). The host owns the machine, input sampling and scheduling.

## Presets

`Chip8()` retains the original default behavior. `Chip8(Profile)` and
`Chip8::preset(Profile)` select the following explicit opcode semantics:

| Behavior | `legacy` | `cosmac_vip` | `chip48` |
|---|---|---|---|
| Shift source `8xy6/8xyE` | VX | VY | VX |
| `Fx55/Fx65` advance I | none | X+1 | X |
| `Bnnn` offset register | V0 | V0 | VX (X is address high nibble) |
| Sprites extending past edges | wrap | clip | clip |
| `8xy1/2/3` clear VF after result | no | yes | no |
| Draw waits for host vblank | no | yes | no |
| `Fx0A` key policy | held | release | held |

Coordinates are reduced modulo 64/32 before clipping. Transfer and `Fx1E`
address updates wrap to 12 bits; memory accesses spanning the end trap before
writing. `increment_i_by_x` takes precedence over `increment_i` when both custom
switches are enabled. Arithmetic snapshots both operands before writing VF,
including when VF is the source or destination. Unknown opcodes are rejected;
`0nnn` machine-code calls and SCHIP's zero-height sprites are unsupported.

The CHIP-48 increment-by-X behavior is deliberate: it reproduces the original
off-by-one transfer behavior, rather than the later SCHIP 1.1 unchanged-I rule.
These are **selected classic-opcode presets**, not complete VIP or HP48 emulation.
The CHIP-48 held-key policy is the existing host-input approximation; it does not
claim to reproduce HP48 keyboard handling.

References for these choices:

- [CHIP-8 extensions, original CHIP-8 and CHIP-48 sections](https://chip-8.github.io/extensions/):
  shifts, logic VF reset, transfer increments, jump offsets, clipping and draw wait.
- [Laurence Scotford, COSMAC VIP keyboard input](https://www.laurencescotford.net/2020/07/19/chip-8-on-the-cosmac-vip-keyboard-input/):
  VIP waits for release of its selected key; hardware debounce/beep timing is not modeled.
- [Timendus test suite](https://github.com/Timendus/chip8-test-suite/tree/742e9eac9f5d44466c1c9d83fcaa8158c6683cb1):
  pinned Corax+ opcode and flags programs and independent output-glyph expectations.

## Input and scheduling

Set `keys[0..15]`, call `step()`, and inject `tick_60hz()` at the host's timer rate.
`step()` executes at most one instruction. `running` means that instruction
completed; `waiting` means `Fx0A` is blocked; `waiting_vblank` means a draw is
blocked. Blocked instructions retain PC and leave their destination/display/VF
unchanged. Timers change only on host ticks, even while an instruction is blocked.

The configurable `KeyWait` policies are:

- `held`: choose the lowest currently held key immediately; wait if none.
- `press`: snapshot keys on entering the wait; accept a subsequently sampled
  false→true transition. A key held on entry must be released and pressed again.
- `release`: select the lowest held key (including one held on entry), then
  complete only after that specific key is sampled released. Other key releases
  do not complete it.

Transitions occurring entirely between calls are not observed. This is sampled
input, not an event queue. There is no physical debounce or audio synthesis.

VIP drawing waits for a tick **after** the draw instruction first blocks. Ticks
are not banked before a draw or accumulated as credits. Once released, that draw
executes once; the next draw must wait for another tick. This models vblank gating,
not instruction-cycle timing or display presentation. The host must bound calls
and continue supplying ticks while waiting.

```cpp
using C = metavm::machines::Chip8;
C machine(C::Profile::cosmac_vip);
if (!machine.load(rom)) { /* oversized program */ }
// Within the host's event loop, update keys, advance bounded instructions,
// and call machine.tick_60hz() on each scheduled 60 Hz tick.
auto status = machine.step();
```

`load` resets registers, memory, display, timers, PRNG and pending waits while
preserving `quirks`. Oversized input returns false without changing state.
A different successfully fetched PC/opcode or key-wait policy observed by `step`
cancels a pending wait. Other public-state edits are read when the instruction resumes. Avoid
concurrent access; the object provides no internal synchronization. Invalid
profile enums throw `std::invalid_argument`. Existing status enum values and the
first four `Quirks` aggregate fields retain their order/defaults; added private
wait state changes object layout, so rebuild C++ consumers.

## Verification and limits

`tests/refinement.cpp` covers each preset, transfer boundaries, flags with VF
operands, jumps, clip/wrap, collision, key transitions, reload/cancellation,
timer ticks and vblank waits. Public stack-state corruption is checked on return.
`tests/chip8_roms.cpp` runs two pinned external ROMs under all three profiles,
checking 22 Corax+ and 47 flags status glyphs against the upstream source.
Fixtures, complete flattened Octo sources, licenses and hashes live in
`tests/fixtures/chip8/`; no network is needed during testing.

Full upstream quirks/keypad/beep suites, SCHIP/XO-CHIP, cycle accuracy, hardware
input/audio and presentation scheduling remain outside this implementation.
