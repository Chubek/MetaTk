# Sixth implementation pass: two executable closure VMs

SECD and Krivine now have independent native C++17 interpreters rather than only
catalog identities. There are **seven executable native subsets and 42 catalog-only
profiles**, still 49 catalog entries in total. Portable example factories retain
their original meaning and are not relabeled as native loaders.

## Added APIs

- `VMLibrary/SECD/SECDMachine.hpp`: versioned unary/i64 control-block programs,
  lexical closures, strict AP/RET, SEL/JOIN, arithmetic and explicit STOP.
- `VMLibrary/Krivine/KrivineMachine.hpp`: versioned closed de Bruijn term DAGs,
  App/Abs/Zero/Succ evaluation to weak-head abstraction closures.
- Both expose immutable-input `verify`/`run` overloads with independent code,
  stack, environment, depth and step limits; SECD also bounds dump continuations.
- Both return owned program/environment snapshots, using flat indices to avoid
  recursive destruction. They add no runtime dependency or historical file loader.
- `closure-vm-demo` is built and installed as a runnable example of both APIs.

[CLOSURE_VMS.md](CLOSURE_VMS.md) is the normative profile/ownership/resource
contract and lists semantic references. Profile version 1 is MetaVM's explicit
in-memory representation, not an invented encoding for Lispkit or another system.

## Integration and verification

The catalog generator now emits the correct native-header includes, coverage
statuses, descriptions and limitations. The schema accepts the two new subset
statuses. Native sources remain outside generator ownership. Header self-containment,
catalog checks, fallback builds, CMake tests and installed SDK tests include both
machines. C factories, all 49 portable examples and prior regression suites remain.

Tests cover escaping captures, lexical shadowing, nested call/selection restoration,
integer wrap/division edges, wrong types, malformed opcodes/versions/addresses,
resource boundaries, divergent self-application, lazy discard of Omega and deep
syntax. A test-only compiler evaluates 64 terminating lambda selector programs
through both machines against known results. See [VALIDATION.md](VALIDATION.md)
for observed commands and outcomes.

## Remaining scope

SECD lists and DUM/RAP, Krivine full normalization/control operators, external
format loaders, guest GC and source-language compilers remain absent. These
interpreters are hand-written, not generated native decoders. MetaTeX/libdrm,
non-Linux validation and the supplied MetaML leak finding remain open. Existing
SSAVM/CHIP-8 semantics, dependency bytes and historical reports/archives are retained.
