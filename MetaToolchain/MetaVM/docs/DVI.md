# DVI format 2 execution manual

`VMLibrary/DVI/DviMachine.hpp` supplies a real binary DVI version-2 decoder and
integer-coordinate interpreter. `vmweave/DviReplay.hpp` builds a verified
Machinery event-replay module from its output. `dvi-run` combines both paths.
It interprets every defined standard opcode family. Rendering, font discovery,
and driver-specific special commands remain host responsibilities.

The semantic reference is Donald E. Knuth, *The DVItype processor*, version 3.6,
sections 13–20 (file format), especially section 15 (instructions), section 18
(fonts), and sections 19–20 (postamble). The public-domain WEB source is maintained
by CTAN as `systems/knuth/dist/texware/dvitype.web`; its formatted reference is
`info/knuth-pdf/texware/dvitype.pdf`. No historical opcode encodings are invented.

## Running a document

From the repository root:

```sh
cmake -S MetaVM -B out-vm -DCMAKE_BUILD_TYPE=Release
cmake --build out-vm -j4
out-vm/dvi-run rules.dvi
out-vm/dvi-run --metrics widths.tsv text.dvi
out-vm/dvi-run --threaded --metrics widths.tsv text.dvi
```

Output is one line per visible rule, glyph, special, or page boundary. Lines give
the source byte offset, event kind, zero-based page index, and integer `h`/`v`
position. Glyphs include font number, character number and advance; rules include
height and width. Specials retain arbitrary bytes and print as lowercase hex.
No special is executed by the CLI. The CLI routes events through VMWeave's
verified module and a bound host callback in either Machinery engine.

`widths.tsv` contains decimal `FONT CHARACTER WIDTH`, one row per character,
with whitespace separators and optional `#` comments. Widths must already be
scaled integer DVI units for the font's definition. Zero and negative widths are
valid. Duplicate keys, invalid integers, widths outside i32, rows longer than
1023 bytes and more than 1,000,000 rows fail. Font number identifies a particular
scaled font in this document. A missing SET-character metric fails; it never
silently becomes zero. PUT characters do not require an advance. Fontless,
rule-only documents need no metric table.

```text
# font  character  scaled-width
0       65         491521
0       66         464215
```

Exit codes are 0 success, 1 invalid document/metrics or execution failure,
2 usage error, 3 file/stream I/O failure. A sink failure can leave a printed
prefix. Usage accepts one input path, optional `--metrics PATH`, and `--threaded`.

## Native API and result

```cpp
#include "VMLibrary/DVI/DVI.hpp"
using D = metavm::machines::DviMachine;
D::Limits limits;
auto document = D::run(bytes, [](const D::Font& font, int64_t character) {
    return lookup_scaled_advance(font, character); // host-supplied int32_t
}, limits);
```

The metric function is application code, not supplied by this example. The API
does not open font paths, trust font area strings as filesystem paths, parse TFM,
or perform approximate font scaling. A host that reads TFM must implement its
scaling and checksum policy correctly. Font definitions expose the number,
checksum, scale, design size, area and name needed by that provider.

Input bytes and callbacks are borrowed without mutation. Results own header,
fonts, page counter arrays, and events (including special bytes). An event's
position is its position before output/advance. `advances` distinguishes SET
and PUT glyphs/rules; a PUT glyph's `width` is zero because no metric is requested.
Non-positive rules produce no rule event, but SET_RULE still advances by its
signed width. Page-begin positions are zero; page-end positions expose final
movement state. Events preserve file order, without sorting by coordinates.

Malformed input, exhausted limits and signed-coordinate overflow throw
`std::runtime_error` with a byte offset. Allocation failure propagates
`std::bad_alloc`; metric exceptions propagate unchanged. A failed call returns
no partial result. Structural decoding finishes before metric callbacks start;
metric providers may still have observed calls if a later dynamic error occurs.
Separate calls share no mutable state. Thread safety of the supplied metric
function is the caller's responsibility.

The header exposes `numerator`, `denominator`, `magnification`, `comment`,
postamble maximum dimensions and declared stack depth. Physical length in meters
is `coordinate * numerator / denominator * magnification / 1000 * 1e-7`.
There is no pixel conversion or rounding. The rendering host chooses page origin
and device rounding. Postamble maximum page dimensions are metadata; the
interpreter does not recompute glyph bounding boxes or enforce those dimensions.

## Instruction contract

All multi-byte fields are big-endian. Distances are signed two's complement.
Character/font numbers are unsigned for 1–3-byte forms and signed for 4-byte forms.
The six position registers are signed i32. Overflow fails rather than wrapping.

| Opcodes | Instruction family | Behavior |
|---|---|---|
| 0–127, 128–131 | SET_CHAR, SET1–SET4 | Emit glyph, advance `h` by its supplied metric |
| 132 | SET_RULE | Emit positive-height/width rectangle at `(h,v)`, then add width to `h` |
| 133–136 | PUT1–PUT4 | Emit glyph without movement |
| 137 | PUT_RULE | Emit positive rectangle without movement |
| 138 | NOP | Do nothing; allowed between commands, including postamble |
| 139 | BOP | Read ten counters and previous-page pointer; reset positions/stack/font |
| 140 | EOP | End page; stack must be empty |
| 141–142 | PUSH / POP | Save/restore `(h,v,w,x,y,z)`; font is not saved |
| 143–146 | RIGHT1–RIGHT4 | Add signed distance to `h` |
| 147–151 | W0 / W1–W4 | Reuse `w`, or assign signed `w`, then add it to `h` |
| 152–156 | X0 / X1–X4 | Reuse/assign `x`, then add it to `h` |
| 157–160 | DOWN1–DOWN4 | Add signed distance to `v` |
| 161–165 | Y0 / Y1–Y4 | Reuse/assign `y`, then add it to `v` |
| 166–170 | Z0 / Z1–Z4 | Reuse/assign `z`, then add it to `v` |
| 171–234, 235–238 | FNT_NUM / FNT1–FNT4 | Select an already defined font |
| 239–242 | XXX1–XXX4 | Emit opaque special bytes with current position |
| 243–246 | FNT_DEF1–FNT_DEF4 | Define font; check repeated postamble definition matches |
| 247 | PRE | Require first byte, format identifier 2 and positive unit/magnification fields |
| 248 | POST | Check final page pointer, matching units, page count and stack bound |
| 249 | POST_POST | Check post pointer, identifier 2 and at least four trailing 223 bytes |
| 250–255 | Undefined | Reject |

Rules' `(h,v)` is the bottom-left corner; height extends upward. Character
coordinates refer to the font's reference point, not its bounding-box corner.
BOP clears the selected font, even if a preceding page selected one. Fonts must
be defined before selection and selected before a glyph. Used fonts must have
matching definitions in the body and postamble. Duplicate definitions within
either region fail. Unused postamble-only fonts are accepted. Scale must be
positive and less than 2^27; design size must be positive signed i32. Font names
and comments retain their original bytes.

The decoder checks all operand/payload truncations, command placement,
previous/final page links, stack underflow/balance, and repeated header fields.
It accepts a zero-page document with final-page pointer −1 and allows any trailer
length of at least four bytes; file length need not be divisible by four.
The declared stack is an upper bound; it may overstate the observed depth.
Page count must match exactly. XDV, pTeX direction commands, and other versions
are rejected rather than treated as standard DVI or NOP.

## Resource limits

| Limit | Default |
|---|---:|
| Input bytes | 67,108,864 |
| Decoded commands, including preamble/NOP/font records/trailer command | 1,000,000 |
| Pages | 65,535 |
| Distinct fonts | 4,096 |
| Simultaneously saved positions | 65,535 |
| Output events | 1,000,000 |
| Aggregate special payload bytes | 16,777,216 |

Zero is a literal limit; unused resources consume none. Structural decode and
execution are iterative and contain no guest-directed host recursion. Limits
bound logical resource counts, not precise RSS. Input, decoded instructions,
font metadata and output events coexist while executing. Metrics-provider
allocation and work are outside these limits. Trailing signature bytes consume
input budget rather than one instruction per byte.

## VMWeave and Machinery integration

```cpp
#include "vmweave/DviReplay.hpp"
vmweave::DviReplay replay(std::move(document), [](const D::Event& e) {
    render_event(e); // application-defined sink
});
auto module = replay.module();
metavm::VM vm;
mvm_error error{};
metavm::check(replay.bind(vm.get(), &error), error);
auto count = vm.run(module); // integer event count
```

`DviReplay` owns a result snapshot and sink. It has a stable address and cannot be
copied/moved; it must outlive every VM bound to it. Binding registers a single
arity-1 `dvi_event` import. The generated module passes an event index for each
event and returns the event count. Source size is independently capped at 16 MiB
by default; `module(max_source_bytes)` can change that cap. Machinery's own
module/register/step limits also apply. No resource limits are raised implicitly.

Index/type checks and exception translation prevent C++ exceptions escaping
the C callback. The callback reports allocation failure as `MVM_OOM`, but
Machinery deliberately normalizes all failed host callbacks to `MVM_HOST_ERROR`
at the `mvm_run` boundary, preserving the detailed diagnostic. A sink or VM
failure may leave externally observed events;
there is no rollback. Immutable documents can be read concurrently, but sharing
a mutable sink requires synchronization. A serialized replay module contains
event indices, not the document; bind it to the matching retained snapshot.

This is concrete program-specialized event replay, not a DVI decoder generated
by Definition IR or a new MetaML parser. The historical DVI portable example
still returns 42 and is explicitly separate from this native loader.

## Verification

`tests/dvi.cpp` covers all standard opcode families and operand widths, signed
edges, font selection versus POP/page reset, invisible rules, binary specials,
page links, duplicate/missing fonts, truncated prefixes, exact limit boundaries,
coordinate overflow and 4,096 fixed-seed mutations. It checks replay through both
Machinery engines and a serialized/deserialized module, plus sink failures.

`tools/test_dvi.py` covers CLI usage, traces, metrics and I/O failures. When `tex`
and `dvitype` are available it generates a real rule-only document and checks the
native dimensions against DVItype. It also uses DVItype's scaled advances for a
real `cmr10` text document and checks glyph offsets/positions and final `h` in
both engines. Otherwise it reports the independent-oracle
check as skipped. Header self-containment and relocated SDK tests cover both
native loading and replay. No fonts, display hardware or external parser are
required by the core tests.
