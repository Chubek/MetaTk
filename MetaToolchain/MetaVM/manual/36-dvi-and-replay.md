# Chapter 36 — DVI version 2 and event replay

[Previous](35-krivine-machine.md) · [Contents](README.md) · [Next](37-vmdesc-and-metatex.md)

## Decoder contract

`metavm::machines::DviMachine` decodes standard DVI identifier 2 and interprets
integer coordinates. Its semantic source is Knuth's *DVItype processor* v3.6,
especially format/instruction/font/postamble sections 13–20. It rejects XDV,
pTeX extensions, unknown versions, and undefined opcodes instead of guessing
compatible behavior.

`run(bytes, metrics, limits)` borrows immutable input and returns an owned
Result containing header, fonts, pages, events, instruction count, and peak
stack. No filesystem/font discovery, rasterization, approximate scaling,
special-command execution, or document rendering is performed.

## Implemented opcode families

| Opcodes | Behavior |
|---|---|
| 0–131 | SET character, including 1–4-byte explicit forms |
| 132 | SET_RULE; advance h even when dimensions make it invisible |
| 133–136 | PUT character without advance |
| 137 | PUT_RULE without advance |
| 138 | NOP |
| 139–140 | BOP/EOP page boundaries |
| 141–142 | Push/pop six position registers |
| 143–146 | RIGHT signed distance |
| 147–156 | W/X assign-or-reuse horizontal spacing |
| 157–160 | DOWN signed distance |
| 161–170 | Y/Z assign-or-reuse vertical spacing |
| 171–238 | Font-number and explicit font selection |
| 239–242 | Opaque special payload |
| 243–246 | Font definitions |
| 247–249 | Preamble, postamble, trailer |
| 250–255 | Reject |

Operands are big-endian. Distances are signed two's complement. Character/font
numbers are unsigned in 1–3-byte forms and signed in 4-byte forms. Coordinates
`h/v/w/x/y/z` are signed i32 and overflow fails, rather than wrapping.
POP restores positions but not the selected font; BOP resets both positions
and selected font. Glyph positions precede advance. Positive rules report a
bottom-left corner with height extending upward.

## Fonts, metrics, and result events

`Metrics` is a callback `int32_t(const Font&, int64_t character)` returning an
advance already scaled into integer DVI units. SET glyphs require it; PUT
glyphs do not advance. Zero/negative widths are valid. No absent metric is
silently approximated as zero. Font metadata retains number, checksum, scale,
design size, area, and name bytes; never trust area/name as authorized paths.

Fonts must be defined before selection and selected before glyphs. Used fonts
need matching body/postamble definitions; duplicates or inconsistencies fail.
Scale must be positive and less than 2^27; design size must be positive signed
i32. Postamble maximum page dimensions are metadata, not recomputed glyph bounds.
Structural decoding completes before metric callbacks start, so a broken
trailer cannot cause early metric side effects. Later dynamic/metric failures
can still occur after callbacks have been observed.

Events are `begin_page`, `glyph`, `rule`, `special`, and `end_page`, carrying
source byte offset, page index, position, applicable font/character/dimensions,
advance flag, and special bytes. Embedded NUL is preserved in opaque strings.
Events remain in source order; there is no coordinate sorting or renderer.

## Structural validation and budgets

The decoder checks truncation, command placement, page links/counts, positive
unit fields, matching postamble units, stack balance/depth declarations, fonts,
and the POST_POST pointer/version/signature. A zero-page document is valid;
the trailer needs at least four 223 bytes, not a guessed file-length modulus.

| Limit | Default |
|---|---:|
| Input bytes | 64 MiB |
| Decoded instructions | 1,000,000 |
| Pages | 65,535 |
| Fonts | 4096 |
| Saved-position depth | 65,535 |
| Events | 1,000,000 |
| Aggregate special bytes | 16 MiB |

Zero is literal. Errors identify byte offsets and throw `std::runtime_error`;
allocation and metric exceptions propagate. Header physical scaling is
`coordinate * numerator / denominator * magnification / 1000 * 1e-7` meters;
pixel rounding belongs to the host.

## Machinery event replay and CLI

`vmweave::DviReplay(document, sink)` owns its snapshot/sink, cannot be copied
or moved, and must outlive bound VMs. `module(max_source_bytes)` builds a
verified zero-argument replay module, default source ceiling 16 MiB.
`bind(vm, &error)` registers arity-1 `dvi_event`; the module passes each event
index and returns the event count.

Replay checks index types/bounds and translates sink exceptions inside the C
callback. It preserves no rollback for external effects. Serialized replay
modules contain indices, not the document: bind to the matching retained
snapshot. This is specialized event replay, not a generated DVI decoder.

```sh
out-manual/dvi-run rules.dvi
out-manual/dvi-run --threaded --metrics widths.tsv text.dvi
```

Metrics rows are decimal `FONT CHARACTER WIDTH`, optional `#` comments,
maximum 1023 bytes/row and 1,000,000 rows; duplicates fail. Exit codes are
0 success, 1 invalid input/execution, 2 usage, 3 I/O. Full contracts and
TeX/DVItype oracle details are in [DVI.md](../docs/DVI.md).
