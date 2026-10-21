# XBind validation regimen

The current suite implements the first executable slice: strict spec contracts,
deterministic JSON IR, compiler-verified signatures and Lua/DomLuaBind generation.
General header parsing, records, callbacks, buffers, automatic ownership and eight
other language emitters remain explicitly incomplete.

## Implemented acceptance gates

- All repository specs load under stock Lua and QaMRpp; the optional Jiterati
  checkout is a C++ header inventory, not a fabricated C ABI.
- Every nonempty C spec emits exact-signature assertions and compiles against its
  real headers. A deliberately wrong prototype must fail compilation.
- JSON IR retains typedefs, target options, ownership, nullability and feature
  tags, and is byte-identical across repeated invocations.
- Duplicate entries, sparse/keyed arrays, malformed constructor kinds, invalid
  nullability and nonempty unsupported modules fail explicitly.
- Driver and loadable QaMRpp module share the same constructors/validator.
- Lua defaults, library overrides and target overrides feed the real emitter.
- Hello and self-binding modules compile through DomLuaBind and execute under
  Lua 5.4; range, arity, type, nil and embedded-NUL failures are exercised.
- Unsupported lowering fails with empty stdout, never a partial/fake wrapper.

The stages below retain the roadmap; their "checks to add" entries are not
claims of implemented general-purpose parsing or all-target emission.

## Stage 0: scaffold integrity

Goal: prove that the tree has enough structure for contributors to start work.

Checks:

- `xbind --check-layout <root>` succeeds.
- `xbind --check-spec hello <root>/examples/hello` succeeds through QaMRpp.
- `xbind --dump-spec Dogfooding <root>` succeeds and lists every backend.
- `python tests/regimen.py --root <root> --cli <built-xbind>` succeeds.
- Every supported target has a compiler directory, DomBind directory, and default
  typedef map.
- `XFeats.schema.json` parses as JSON.
- `examples/hello/hello.xbind` targets D, Go, Java, Lua, NodeJS, OCaml,
  Python, Ruby, and Rust.

## Stage 1: Lua spec contract

Goal: make `.xbind` files deterministic before executing user code in the full
driver.

Current checks:

- Load `lxbind/lxbind.lua` with a stock Lua interpreter.
- Load `examples/hello/hello.xbind` through QaMRpp and verify it returns a
  manifest table.
- Reject missing library names, empty target names, malformed params, duplicate
  function names, and type overrides without C/native sides.

Repository specs should remain deterministic under both the stock Lua helper and
the QaMRpp helper. The committed all-backend specs are documented in
`MANUAL.md`.

## Stage 2: C surface ingestion

Goal: establish the XBind IR independently of target languages.

Checks to add next:

- Parse `examples/hello/hello.h` and preserve source spans.
- Match spec functions to C declarations by name.
- Match every committed repository spec function to its declared headers.
- Diagnose missing declarations, ambiguous declarations, unsupported C types, and
  conflicting ownership annotations.

## Stage 3: type-map convergence

Goal: make target types explicit and overrideable.

Checks to add next:

- Parse each `typedefs/*-Default.sexpr` file.
- Apply spec-local overrides after defaults.
- Report the final C-to-native map per target.
- Fail on unmapped ABI-visible types unless the spec marks them opaque.
- Keep the repository specs on default-mapped scalar/string types or explicit
  opaque/status/integer typedefs until records, callbacks, and buffers land.

## Stage 4: DomBind lowering

Goal: make one target work end-to-end before expanding.

Recommended first target: Lua, because `domluabind` has a compact native surface.

Checks to add next:

- Lower the hello example into a target-neutral XBind IR dump.
- Generate Lua glue through `compilers/Lua.c` and `dombind/domluabind`.
- Compile the generated module and run a Lua smoke script.
- Preserve the `Dogfooding.xbind` self-binding as a second, callback-free IR
  input once the IR dump exists.

## Stage 5: all-target smoke matrix

Goal: keep the SWIG-like promise honest across the configured targets.

Checks to add next:

- Build generated bindings for every target whose SDK is available.
- Run one smoke test per target for functions, records, strings, buffers,
  callbacks, error paths, and ownership transfer.
- Skip only when the native SDK is absent; compiler/runtime failures are hard
  failures.

## Definition of done for the first real slice

- `examples/hello/hello.xbind` generates and runs in at least one target.
- `tourchstone/xbind/Dogfooding.xbind` generates a usable wrapper for the
  stable `xbind.h` query surface.
- The target compiler emits through DomBind rather than hand-written glue.
- Default typedefs and user overrides are both exercised.
- Generated traces show the selected spec, headers, targets, type maps, and
  emitted files.
