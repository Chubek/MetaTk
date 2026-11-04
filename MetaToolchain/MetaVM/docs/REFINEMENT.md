# VM refinement regimen

Apply this process to each native implementation before changing coverage status.
The current DVI addition follows it; the remaining legacy targets are tracked in
[VMS.md](VMS.md), with one explicit compatibility requirement per identity.

1. Establish the actual baseline and inspect local contracts. Preserve existing
   semantic tests and portable-example identity. Pick a documented version or
   explicit in-memory profile; obtain authoritative semantics before inventing
   opcodes, values or a loader. A name alone is insufficient.
2. Write the native contract: representation, instruction semantics, malformed
   inputs, arithmetic edge behavior, ownership, thread safety and independent
   resource ceilings. Define external services such as font metrics explicitly.
3. Implement in the native library, preserving C11 Machinery and optional
   MetaML/MetaTeX integration. Reject unknown operations. Keep verified Machinery
   authoring/replay distinct from native guest decoding.
4. Test semantic boundaries: each opcode family/operand width, signed limits,
   control-state restoration, static structure, dynamic traps and exact resource
   boundaries. Use an independent producer/interpreter where available. Add
   deterministic hostile mutations without treating them as a complete fuzzer.
5. Verify VMWeave integration through both Machinery engines, serialization,
   host errors and callback lifetime rules. Check public header self-containment
   and build/run installed consumers after relocating the installation prefix.
6. Run full GCC and Clang Release builds with `-Wall -Wextra -Wpedantic -Werror`,
   and ASan/UBSan builds. Record environment restrictions; a disabled leak
   detector is not a passed leak check. Do not weaken a regression to pass.
7. Change generator-owned metadata through the generator, regenerate, and verify
   byte-stability. Keep the schema, manifest, README and per-VM manual aligned.
   Preserve unresolved limitations and distinguish subset support from a full
   platform runtime.

For DVI the independent oracle is a real TeX-generated rule-only document,
checked with DVItype. The native tests also exercise signed character/font
numbers, all position registers, POP/font separation, page reset, invisible
rules, opaque binary specials, pointer chains, trailers, invalid prefixes,
resource boundaries and 4,096 fixed-seed mutations. Metrics are supplied by the
host; no zero-width fallback is accepted. The executable manual check covers all
38 directory identities, so adding or renaming one requires an explicit entry.

This regimen is not a claim that the outstanding 33 legacy profiles have native
implementations, that the four previous legacy subsets are complete platforms,
or that fixed-seed mutations replace sustained fuzzing. See the current report
for observed validation and remaining work.
