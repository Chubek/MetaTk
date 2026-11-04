# Chapter 40 — Compatibility limits and development roadmap

[Previous](39-validation-and-troubleshooting.md) · [Contents](README.md)

## Current capability matrix

| Layer/profile | Implemented boundary | Not implied |
|---|---|---|
| Machinery | Verified C11 register VM, objects, host imports, two engines | Native JIT, process sandbox, arbitrary ABI FFI |
| VMWeave Builder | Verified Machinery source authoring | Automatic historical ISA implementation |
| Definition v1 | Fixed-width, program-specialized static-target lowering | Reusable decoder, indirect/self-modifying code |
| MetaML | Real optional Session-based authoring/Definition embedding | Sandboxed arbitrary build-time programs |
| VMDesc | Source adapter to actual MetaTeX Engine | Validated complete renderer/dependency graph |
| Catalog | 49 identities and portable examples | 49 native runtimes |
| CHIP-8 | Selected classic opcode profiles, sampled waits/ticks | Full VIP/HP48 hardware, SCHIP/XO-CHIP/audio |
| SSAVM | i64/immutable-array interpreter, bounded optional analysis | Complete paper ISA, SafeTSA, dominance/termination proof |
| DRMVM | Software host-import surface; optional dumb allocation/upload | Compositor, modesetting, DMA-BUF/synchronization |
| JVM | Integer raw Code arrays | Class loading, objects, JVM standard platform |
| FORTH | Whole-input integer words/colon definitions | Interactive standard Forth environment |
| SECD | Unary strict lexical closures and i64 control blocks | Lispkit images, lists, DUM/RAP, GC |
| Krivine | Closed de Bruijn terms to call-by-name WHNF | Open terms, full normalization, call-by-need |
| DVI | Standard identifier-2 decode and integer events | Font discovery/rasterization, special execution, XDV |

The native count remains eight, with 41 catalog-only profiles. A factory that
returns `42`, a descriptor file, a compiled metadata translation unit, and a
native guest loader are different achievements. Release notes and user-visible
errors must preserve those distinctions.

## Compatibility rules to preserve

Keep the C11 core and C++11 Machinery wrapper available independently of the
C++17 framework and optional C++20 integrations. Retain existing direct-write
APIs alongside staged alternatives. Preserve module ownership, callback state
lifetimes, atomic batch-registration rollback, defined arithmetic, and immutable
module sharing. Never mutate a running module or reenter its VM.

Wire version 1 is not a C structure ABI. New wire semantics need versioning,
while SECD/Krivine in-memory versions separately describe their declared
profiles. Native-format additions require an authoritative version/encoding
choice, not just a guessed historical name. Preserve legacy directory/symbol
compatibility, including `Smaltalk-80`, unless an explicitly reviewed migration
adds rather than silently replaces public paths.

## Sensible future work

Prioritize independently testable capabilities:

1. Generate a reusable decoder only after extending Definition semantics and
   proving parity with its specialized lowering path.
2. Add one versioned guest loader/runtime subset at a time with independent
   executable inputs, malformed cases, budgets, and coverage documentation.
3. Expand SSAVM instructions/functions/analysis while retaining simultaneous
   PHIs, immutable array versions, edge-aware facts, and explicit work limits.
4. Extend closure profiles only with pinned semantics for recursion, lists,
   effects, image formats, normalization, or GC.
5. Validate real MetaTeX output devices and libdrm hardware integration before
   advertising them as tested deployments.
6. Address supplied MetaML allocation findings without substituting parser or
   diagnostic stubs; preserve dependency provenance.
7. Add non-Linux CI, platform-specific deployment evidence, and per-machine
   selective builds as implementations grow.

These are development directions, not promised release features. A team must
choose the exact language/runtime editions and host environments for each.

## Maintain the manual as part of the contract

When behavior changes, update the public header, regression tests, generated
metadata source, coverage, and affected chapter together. Verify examples and
relative links. Keep older refinement reports clearly historical rather than
deleting provenance or treating their resolved gaps as current facts.

A release claim should name tested configurations, failures/skips, supported
input versions, and remaining limitations. Passing the current suite does not
make the entire multi-runtime catalog complete or production-ready.

**References:** [implementation contract](../AGENTS.md), [coverage](../docs/COVERAGE.md), [refinement plan](../docs/REFINEMENT.md).
