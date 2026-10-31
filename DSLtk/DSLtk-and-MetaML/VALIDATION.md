# Validation of the supplied edition

Validated on 2026-10-31 against the exact four source snapshots identified in `SOURCE-MAP.md`.

## Completed checks

- LuaLaTeX compilation through the supplied `texlua build.lua`, three passes, without shell escape.
- PDF page count: **300 physical pages**, 6 by 9 inches.
- 24 chapters, each with five paired workshops: **120 C++ programs and 120 D programs**.
- Four additional complete native case-study programs, giving **244 native programs** in total.
- All **120 C++ workshop bodies compiled and executed successfully** in the aggregate runner using GCC, `-std=c++20 -O0 -pthread`.
- Both standalone C++ case programs compiled and executed successfully.
- Formula acceptance checks: valid reduction, zero-step budget status, seven malformed/unsupported input fixtures, and 100 fixed-seed before/after semantic-preservation tests using unsigned 64-bit modular arithmetic.
- Policy acceptance checks: correct policy accepted; an always-allow candidate and an ill-typed candidate rejected; active generation preserved.
- TeX log: no overfull horizontal or vertical boxes and no unresolved references after the final passes.
- Rendered-page visual inspection: title, paired workshop pages, chapter opening, longer case-study listings and final-chapter layout.
- Vendor SHA-256 values recorded; copies preserve uploaded bytes.

The text extraction contains approximately 55,000 whitespace-separated tokens, including code, captions, contents and repeated page furniture. This is not a claim of 55,000 words of narrative prose.

## Scope and limitations

- **D compiler unavailable**: D examples and case programs were reviewed against the supplied module APIs but were not compiled or executed here. `make test-d` runs them on a machine with LDC or DMD and reports missing tools as NOT RUN. D validation is not counted as a passed check.
- The supplied MetaML external standard-library tree is absent. No successful external prelude load or external metalib module is claimed as tested. Missing-library discovery is covered.
- C++ workshops were compiled as one aggregate translation unit for efficient validation. Each standalone program has the same imports and its own main body; separate per-file builds are documented.
- Case programs are synchronous tools with explicit scope, not production network services. Input-size limits, process containment and concurrent publication are application extensions.
- The guide is source-pinned. Source comments that disagree with implementations are discussed, including Result-returning eval and C++ PEG rule-reference lifetime concerns.

Raw concise outcomes are in `validation/`. Development binaries and temporary TeX output are not needed in the archive; rebuilding recreates `build/`.
