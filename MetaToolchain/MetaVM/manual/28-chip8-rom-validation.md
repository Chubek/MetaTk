# Chapter 28 — CHIP-8 ROM validation and host integration

[Previous](27-chip8-profiles-and-scheduling.md) · [Contents](README.md) · [Next](29-ssavm-execution.md)

## What the pinned suite establishes

`tests/chip8_roms.cpp` runs Corax+ and flags ROMs from Timendus' test suite,
pinned to commit `742e9eac9f5d44466c1c9d83fcaa8158c6683cb1`. It checks 22
Corax+ status glyphs and 47 flags-test glyphs under each of the three presets.
Expected checkmark pixels and positions are derived from upstream source, not
captured from the interpreter being tested.

The ROMs must reach their final self-jump within a fixed attempt budget. The
test injects a 60 Hz tick every ten attempts so VIP draw waiting is exercised
rather than disabled. It validates opcode/flag behavior selected by those
programs, not every extension or physical device characteristic.

```sh
ctest --test-dir out-manual -R metavm_chip8_roms --output-on-failure
out-manual/metavm_chip8_roms tests/fixtures/chip8
```

## Fixture provenance

The fixture directory retains unmodified ROM binaries, complete flattened Octo
sources, upstream revision, SHA-256 values, and licenses. Tests need neither
network access nor an Octo compiler. These fixtures are test data, not installed
or linked library resources. Preserve the GPLv3 suite license and Corax+'s
additional MIT notice when redistributing them.

Do not replace a pinned fixture with a newer upstream file under the same name
without reviewing source differences, hashes, licenses, and expected outputs.
A successful download does not establish a compatible test revision.

## Unit tests complement ROM tests

The framework refinement suite independently exercises:

- Both operands aliasing VF and destination/source flag ordering.
- Transfer boundaries and all three I-increment policies.
- Bnnn register selection, shift sources, clipping/wrapping, and collisions.
- Held/press/release transitions, reload cancellation, and timer behavior.
- Draws waiting for a post-block tick rather than a banked credit.
- Invalid opcodes, invalid profile enums, and corrupted public return-stack state.

Keep these focused checks even when a large ROM seems to cover the same
instruction. A rendered pass screen rarely identifies which boundary regressed.

## Integrating another ROM safely

Identify its intended instruction family and quirk assumptions before choosing
a profile. Bound file size before loading; `load` admits at most 3584 program
bytes. Set a host instruction-attempt budget, not just a count of completed
instructions, because waits may not complete. Keep input and timer delivery
alive during waits and treat terminal error statuses as errors.

For deterministic tests, record initial PRNG state, key samples, tick schedule,
profile/custom quirks, ROM bytes/hash, and observable expected state. For live
applications, use elapsed host time for timers rather than decrementing them
once per guest instruction.

Full upstream quirks, keypad, and beep suites, SCHIP/XO-CHIP, cycle timing,
audio synthesis, and display hardware remain outside current coverage. A ROM
reaching a familiar screen is useful evidence, not a blanket conformance claim.

**References:** [fixture manifest](../tests/fixtures/chip8/README.md), [ROM test](../tests/chip8_roms.cpp).
