# Pinned CHIP-8 regression ROMs

Unmodified artifacts from [Timendus/chip8-test-suite](https://github.com/Timendus/chip8-test-suite),
commit **742e9eac9f5d44466c1c9d83fcaa8158c6683cb1**, retrieved 2026-10-31.
The upstream `bin/` contains the ROMs and their complete flattened Octo sources;
both are retained here. No downloads or Octo compiler are required at test time.

| File | SHA-256 |
|---|---|
| `3-corax+.ch8` | `1c7e14eae14d6d5e1e47693804110354cbc4081defe4e6e5d9167c25ffc7b4b0` |
| `3-corax+.8o` | `2060b98128b41d08e963438270337b53493abada91290d217c39d3682683d70c` |
| `4-flags.ch8` | `f00ddadd37bc878473de0c8f16faecf9985dea39036a3a796d551bc9fec47cfa` |
| `4-flags.8o` | `296a8fc787e28d23de770b44c80defd65f2f4e7739eaa85d204f95c94ecb0d09` |
| `LICENSE` | `3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986` |

The test suite's GPLv3 license is retained in `LICENSE`; Corax+'s additional MIT
notice (corax89 and Timendus) is retained in its source. These are test data,
not linked into or installed with MetaVM's libraries. Upstream build instructions
and preferred source tree are available at the pinned repository revision.

`tests/chip8_roms.cpp` checks all 22 Corax+ status glyphs and 47 flags-test status
glyphs. Positions and expected checkmark pixels are derived from the upstream
source (`image-ok`, `flag-ok`, drawing macros), not our interpreter's output.
Both ROMs must reach their final self-jump within a fixed instruction-attempt
budget. Ten attempts per host-injected 60 Hz tick exercise the VIP draw wait.
This is selected opcode/flag coverage, not the complete upstream compatibility
suite, keypad tests, hardware timing, or SCHIP/XO-CHIP conformance.
