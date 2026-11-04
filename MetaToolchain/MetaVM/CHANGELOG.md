# Changelog

## [Unreleased]

### Added

- Build-time generation of all 49 portable VMLibrary examples as verified binary
  modules and serialized C factories, with compiled-artifact parity/execution tests.
- Non-mutating catalog `--check`, separate `--output-root`, collision validation,
  deterministic/no-op regeneration and failure-preservation regressions.
- Standard DVI format-2 binary interpretation, bounded event output, host font
  metrics, VMWeave/Machinery replay and an installed `dvi-run` command.
- Native/CLI/interoperability regressions, installed-SDK and header checks for
  DVI, plus a generated manual covering all 38 legacy VM identities.
- An explicit VM refinement regimen, compatibility requirements and report.

### Fixed

- Restored the required VMWeave and legacy adapter sources, and supplied the
  missing DVI event-replay adapter used by the runner and installed SDK.
- Catalog regeneration no longer fails because an unrelated/build file is empty,
  rewrites unchanged files, or writes outputs before identity validation completes.
- Unmapped legacy example factories now set diagnostics and distinguish invalid
  output pointers from unsupported machine identities.

### DVI catalog data

- Changed DVI coverage from `catalog-only` to `dvi2-interpreter` and marked its
  native format loader available. The reason is the new tested version-2 loader;
  the semantic reference is Knuth's *DVItype processor* v3.6, sections 13–20.
  Font metrics, rendering, specials and nonstandard-version limitations remain
  explicit. Generator-owned manifest, profile, C++ metadata and coverage/manual
  output were regenerated together. No guest instruction data was fabricated.
