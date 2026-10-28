# 17. Canonical IR and reproducible round trips

Canonical Juniper IR is the interchange format for continuing compilation without
replaying macros. Use `emit`, not `preprocess`, when you need a validated, stable
representation that preserves the backend configuration.

## Start with an explicit profile

<!-- check: juniper -->
```juniper
@backend(compact_add)
  @issue(2)
  @resource(alu, 2)
  @rule(add_small, add(reg, imm(-8, 7)), ADD_SMALL, 1, 1, alu)
@end
@module(round_trip)
@fn(increment, (x))
  @return(add(x, 1))
@end
```

Canonical emission writes `juniper 1;`, the complete expanded backend, then the
MetaIL body. The profile no longer says `extends portable`: inherited rules have
become explicit declarations in their original tie-breaking order. Resources
are emitted in stable name order. Comments and macro definitions are not retained
as a source-preserving editor format.

## Verify idempotence

```sh
"$J" emit "$WORK/round-trip.jun" -o "$WORK/first.junir"
"$J" emit "$WORK/first.junir" --no-preprocess -o "$WORK/second.junir"
cmp "$WORK/first.junir" "$WORK/second.junir"
```

Save the preceding source as `round-trip.jun`. Byte identity after the first
canonicalization is the useful round-trip property. Do not compare canonical
output byte-for-byte with hand-written macro source: they serve different roles.

Test the round trip rather than assuming every emitter corner is covered. In
particular, the current negative-literal unary-negation formatting limitation
and its constant-folding workaround are documented in
[chapter 12](12-integer-semantics.md#current-unary-formatting-corner-and-its-workaround).

The same operation in a C++ fragment is:

```cpp
// compiler is a live juniper::Compiler; module is a checked juniper::Module.
auto first = compiler.emit(module);
if (!first) return 1;
auto parsed = compiler.parse(*first.get(), "canonical.junir");
if (!parsed) return 1;
auto second = compiler.emit(*parsed.get());
if (!second) return 1;
if (*first.get() != *second.get()) return 1;
```

Chapter 20 supplies a complete application around these APIs.

## Know the lossy exports

`metail` exports the program body and discards the Juniper backend profile. If
you import that file later, Juniper supplies the portable backend unless a new
profile is provided. `select`/`schedule` JSON contains machine instructions and
scheduling metadata, not the full source rule set. There is no JSON importer.

```sh
"$J" metail "$WORK/first.junir" --no-preprocess -o "$WORK/body-only.mi"
"$J" emit "$WORK/body-only.mi" --no-preprocess -o "$WORK/portable-again.junir"
```

The final profile can legitimately differ from `compact_add`. Use this export only
when losing the profile is intentional.

## Freeze the right provenance

Store canonical IR alongside configuration metadata when reproducing a build:
source revision, macro definitions/include roots, runtime/kernel revision, pass
list/round count, profile semantics, and tool version. `emit` freezes the expanded
IR; it does not make time-dependent macros or a changing external runtime
reproducible by itself.

[Previous](16-effects-and-traps.md) · [Index](README.md) · [Next](18-optimization.md)
