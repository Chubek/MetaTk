# Fourth-pass test fixtures

Pinned external CHIP-8 test ROMs and complete flattened Octo sources are stored
under `tests/fixtures/chip8/`, with upstream revision, SHA-256 hashes and licenses
in its README/LICENSE. They are test-only, not installed or linked into libraries.
The supplied dependency bytes below remain unchanged.
LeakSanitizer now reports retained MetaML type-checker allocations during the
Definition CLI test; see `docs/VALIDATION.md` for details and reproduction.

# Current dependency validation

The third pass compiled and executed the **unchanged supplied MetaML.hpp** with
the current workspace `/mnt/nvme/metatk/DSLtk/DSLtk.hpp` (SHA-256
`01ce1fe69e1e09f88662946bf9dca19baa7cd7ada9c0ae633d730685f529bfa3`).
It supplies the required diagnostic/token/parser/graph APIs. Configure explicitly
with `METAVM_DSLTK_INCLUDE_DIR`; no DSLtk copy or substitute is vendored here.
The MetaML SHA-256 remains
`1477e56be681541983785eedd5c7eb30e8fad02f77f878a90ba390f7d2326b9a`.
See `docs/ROUND3.md` and `docs/VALIDATION.md`. MetaTeX/libdrm validation remains open.

Historical provenance and earlier dependency observations follow unchanged.

# Supplied inputs and optional dependencies

The existing Machinery implementation was retained from MetaVM.zip. MetaML.hpp
here is the exact supplied MetaML(1).hpp. No substitute DSLtk implementation is
included. Build the MetaML embedding only with the matching C++20 DSLtk header.
The authoring adapter registers typed primitives on metaml::Session; no external
MetaML standard library is required for the bundled authoring examples.

VMDesc targets the actual metatex::Engine API in the supplied MetaTeX.zio ZIP
(the extension does not change its ZIP format). Extract that original archive
and configure its MetaDoc/MetaTeX directory as the external source dependency.
It is not duplicated in this package. That project also needs MetaVect, DSLtk,
MetaML and the optional/external components described by its CMake configuration.
Retain and follow its own AGENTS.md when refining that dependency.

Original input SHA-256 hashes:

```
MetaVM.zip
bf36dabf211256b78026390ed55fe33c6aeb9d36047931a9bc6586d5a39005e2
MetaML(1).hpp (vendored here as MetaML.hpp)
1477e56be681541983785eedd5c7eb30e8fad02f77f878a90ba390f7d2326b9a
MetaTeX.zio
75d1113afe7a278a6665fa3c6fa72f3e7e0aea9e242a2c69c1adad02c02e9334
vm-ssa.pdf (preserved in docs/)
dd19acd2467eb6542093e41afde5d0a68f1f296bcd031b2f26d3def2227c54ee
```

The standalone build has no third-party runtime dependency. The optional Linux
DRM backend needs libdrm headers/library and a caller-authorized KMS device fd.
No matching headers, library or GPU were available for hardware validation.

Supplied artifacts carry their original provenance; this package does not infer
or add a license grant to third-party inputs.

## Second-pass dependency check

An older DSLtk.hpp was located and compiled against the actual supplied MetaML.
Its SHA-256 is 473d6ab3a9356b5f96064f5a02b3f3d4f552909bf4dcf6f95acf532a41780e7f.
It lacks the dsl::Diagnostic/Severity/Span/BasicToken API expected by this MetaML.
A matching version is still required. The incompatible header is not substituted
into the source tree. See docs/ROUND2.md for the observed compatibility outcome.
