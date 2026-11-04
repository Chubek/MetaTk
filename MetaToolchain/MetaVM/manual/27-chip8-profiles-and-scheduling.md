# Chapter 27 — CHIP-8 profiles, input, and scheduling

[Previous](26-chip8-machine.md) · [Contents](README.md) · [Next](28-chip8-rom-validation.md)

## Presets define selected semantics

`Chip8()` preserves legacy defaults. `Chip8(Profile)` and `preset(Profile)`
support `legacy`, `cosmac_vip`, and `chip48`; invalid profile values throw
`std::invalid_argument`.

| Behavior | Legacy | COSMAC VIP | CHIP-48 |
|---|---|---|---|
| Shift source | VX | VY | VX |
| `Fx55/Fx65` I increment | None | X+1 | X |
| `Bnnn` offset | V0 | V0 | VX selected by address high nibble |
| Sprite edges | Wrap | Clip | Clip |
| Logic resets VF | No | Yes | No |
| Draw vblank gate | No | Yes | No |
| Default key wait | Held | Release | Held |

The original CHIP-48 increment-by-X rule is deliberate; do not replace it with
the later SCHIP unchanged-I convention. Coordinates are reduced modulo 64/32
before the clip decision. These profiles are selected opcode semantics, not
full emulations of VIP or HP48 hardware and keyboard timing.

Custom `Quirks` expose `shift_uses_y`, `increment_i`, `jump_uses_x`,
`clip_sprites`, `logic_resets_vf`, `increment_i_by_x`, `wait_for_vblank`, and
`key_wait`. Increment-by-X takes precedence if both increment flags are set.

## Sampled key policies

- `KeyWait::held`: immediately select the lowest currently held key; otherwise wait.
- `KeyWait::press`: snapshot entry state and accept a later sampled false→true
  transition. A key already held must be released and pressed again.
- `KeyWait::release`: select the lowest held key, then wait for that particular
  key to be sampled released. Releasing another key does not complete the wait.

There is no event queue. A transition occurring wholly between `step()` calls
is invisible. Physical debounce, HP48 scanning behavior, and audio feedback are
not implemented. Update `keys` before stepping according to the host event loop.

## Timer and vblank ownership

Call `tick_60hz()` at host-scheduled 60 Hz intervals, including while blocked.
It decrements nonzero delay/sound timers and releases an already-blocked VIP
draw. A tick before a draw blocks is not banked as a future credit. Each gated
draw must first block, then observe a subsequent tick, and then execute once.

`waiting` and `waiting_vblank` retain PC and leave the blocked instruction's
destination/display/VF unchanged. The host must not spin indefinitely waiting
for `running` without servicing input/ticks. Instruction pacing and display
presentation are separate scheduling decisions; ten attempts per tick in a
regression is not a claim of cycle accuracy.

## Host loop design

Use a bounded number of attempts per iteration. Sample keys, inject every due
timer tick, step until the work allowance or a wait/error, then return to the
host event loop. Present the display through the application's own window,
terminal, or framebuffer interface. Generate sound from timer state only if
the application implements its own audio backend.

Successful load cancels pending waits. A different fetched PC/opcode or changed
key-wait policy also cancels a pending wait; other public-state edits are read
when the instruction resumes. Synchronize access to the entire machine object.

**References:** [CHIP-8 contract and sources](../docs/CHIP8.md), [ROM integration](28-chip8-rom-validation.md).
