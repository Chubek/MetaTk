# Markdown subset and AST correctness pass

`markdown` is a read-only format producing the **partial MDAST model** described in `doxir-db/MDAST.doxir`. It is not CommonMark-conformant and does not promise mdast/unist JSON compatibility.

## Implemented reader

- ATX headings (including empty headings), paragraphs, soft/hard line breaks.
- Backtick/tilde fences, matching fence lengths, language/meta fields, blank code lines, and unclosed fences through EOF.
- Explicit blockquotes; single-line ordered/unordered list items; task-list checkboxes.
- Basic single/double emphasis with whitespace-aware delimiters, code spans with exactly matching backtick runs, inline links with simple destinations and optional double-quoted titles (including parentheses inside titles), images with literal alt text.
- Zero-based source line/Unicode-scalar column and UTF-8 byte offsets for every node; end positions are exclusive. CRLF counts as one newline. Positions refer to the **decoded UTF-8 source**, not the original bytes when transcoding Latin-1.

Reference definitions, HTML-looking blocks, and indented code remain `verbatim` nodes with a `reason` attribute. Unresolved references and other unsupported inline syntax remain literal text; they are never silently removed. Triple emphasis runs, intraword underscores, setext headings, entity decoding, escape processing, reference resolution, multiline inline constructs, full HTML block rules, nested/continuation lists, balanced-parenthesis destinations, and CommonMark delimiter precedence are not implemented. CR/CRLF logical line breaks are normalized to LF in text/code values; decoded source bytes remain available separately.

Nesting is limited to 64 parser levels; exceeding it returns `parse_error`. Invalid UTF-8 returns a byte-offset diagnostic. Empty documents are valid. UTF-8 BOMs are ordinary content, not silently stripped. Unmatched code runs stay literal in their entirety. Destinations with nested parentheses or unsupported whitespace remain literal rather than being truncated. A trailing odd backslash run creates a hard break; an even run remains literal (full escape processing is still unimplemented).

## AST and transport

`mdast::parse()` returns a value-type `Node`. `parse_markdown()` additionally returns a `Document` with a `shared_ptr<const Node>` in `tree` and the decoded source in `data`. Copies share an immutable tree. Translations prefer that tree; source-only envelopes from older callers are reparsed for compatibility. Do not mutate a tree through a retained non-const alias after publishing it.

`Node::validate()` checks known kinds, UTF-8 text/attribute strings, leaf/container shape, sorted unique attribute keys, paired positions, endpoint ordering, parent bounds in both byte offsets and line/column coordinates, and a 256-level tree limit. Public attributes are not automatically sorted: callers must sort them before validation or serialization. `ASTVisitor` dispatches all supported kinds including `verbatim`, walks children in order, and supports early termination. Validate externally constructed trees before traversal. `Node::outline()` escapes quotes, backslashes, and control bytes for logging and marks truncation beyond 256 levels.

## Formats and conversion policy

| Source | Target | Policy |
| --- | --- | --- |
| `markdown` | `ir-json` / `ir-xml` | Preserve the parsed AST and metadata; no lossiness permission required |
| `markdown` | `plaintext`, `json-dump`, `xml-dump` | Flatten structure; `allow_lossy` / `--allow-lossy` required |

```sh
build/toolchain/doxtk-convert --from markdown --to ir-json input.md ast.json
build/toolchain/doxtk-convert --from markdown --to ir-xml input.md ast.xml
build/toolchain/doxtk-convert --from markdown --to plaintext --allow-lossy input.md output.txt
```

Flattening keeps text, emphasis/strong contents, code, link labels, image alt text, line breaks, and verbatim text. Block containers join with blank lines; lists use `- ` markers. Formatting, link destinations/titles, checkbox states, ordered-list numbering, and source positions are lost. The result reports that loss in `Result::diagnostics`; CLI warnings go to stderr whether output is a file or stdout. `strict=false` does not replace the explicit `allow_lossy` permission.

`mdast::to_plaintext()` is a direct rendering API and returns a lossiness diagnostic without checking options. Registry conversions enforce permission. Metadata survives translation: Plaintext serialization still rejects nonempty metadata; choose a dump format to preserve it. Use `convert()` or `convert_file_with_diagnostics()` to inspect successful diagnostics. The latter returns the written path on success and retains conversion diagnostics if writing fails. The legacy `convert_file()` convenience function returns only errors; CLI warnings are displayed on stderr.

## Dump contract

`ir-json` contains `ir`, `media_type`, `json_type`, sorted `metadata`, and `root`. Node fields are `kind`, `text` (including empty values), sorted `attributes` entries, `start`, `end`, and `children`. Missing positions are `null`.

`ir-xml` uses a fixed element-name alphabet. Each node contains `value`, `attributes` (`attribute key="..."` entries), `start`, `end`, and `children`; arbitrary attribute keys are data, never XML markup. Positions include `valid`, and valid endpoints carry `line`, `column`, and `offset`. Sorted `metadata` entries preserve keys and values. XML 1.0-forbidden scalars are rejected, not filtered. All strings are UTF-8-validated before scalar decoding. Both dumps are deterministic, write-only inspection envelopes, not general JSON/XML readers or dump deserializers.

The transport identifiers `DoxTkJSON` and `DoxTkXML` describe already-serialized inspection envelopes, not universal JSON/XML ASTs. Format registration remains separate from the source `MDAST` IR.
