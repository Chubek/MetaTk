> [ROUND5.md](ROUND5.md) adds opt-in SSAVM definite-assignment/type checking.
> Conventional SSA dominance and value-range analysis remain future work.

# Fourth implementation pass

This pass addresses CHIP-8 compatibility and installation of the wider framework.
Earlier reports, dependency bytes and `history/round1.zip` are retained.

## CHIP-8

- Added `Profile::{legacy,cosmac_vip,chip48}`, `preset`, and a profile constructor.
- Added logic-VF-reset, increment-I-by-X and host-vblank quirks; the original four
  aggregate switches and default behavior remain available.
- Added sampled held/press/release `KeyWait` policies and appended
  `Status::waiting_vblank`. Host ticks release blocked draws and advance timers.
- Added pending-wait reset/cancellation and bounds validation for a host-mutated
  return stack pointer.
- Added direct semantic/boundary tests and pinned external Corax+/flags ROMs,
  with complete flattened source, hashes and license notices. All 69 status
  glyphs are checked for each of the three profiles.

[CHIP8.md](CHIP8.md) defines the exact semantics and references. Original CHIP-48's
increment-by-X transfer behavior is distinct from later SCHIP. Profiles cover
selected opcode behavior, not full hardware emulation. C++ consumers must rebuild
because wait state extends the object layout.

## SDK packaging

- Added the `MetaVM` config/version package and `VMWeave`, `Authoring`, `Catalog`
  and `Framework` imported targets alongside the existing Machinery package.
- Installed public framework/native/legacy headers, tools, machine descriptions,
  examples and documentation with their relative directory layout.
- Added relative shared-library/tool runpaths and an install-relocate-consume
  regression using actual C11/C++17 executables and emitted C.
- Made CLI preservation tests conditional on the optional Machinery runner.

[PACKAGING.md](PACKAGING.md) gives consumer instructions and optional-dependency
requirements. Static/shared Linux SDKs are exercised; non-Linux packaging and
MetaTeX/libdrm-enabled package configurations remain open.

## Remaining priorities

See [VALIDATION.md](VALIDATION.md) for observed results and commands. Full SSA
dominance/type analysis, reusable native decoder generation, the real MetaTeX
dependency graph, libdrm hardware tests, broader CHIP-8 compatibility and
non-Linux CI remain future work. Forty-four machine profiles remain catalog-only;
portable factories still demonstrate the Machinery build pipeline only.
LeakSanitizer also reported allocations retained by the supplied MetaML type
checker; details and a reproduction command are in `VALIDATION.md`. ASan/UBSan
passes use the established leak-disabled configuration, not a leak-clean claim.
