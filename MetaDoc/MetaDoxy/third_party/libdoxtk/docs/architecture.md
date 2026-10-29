# Architecture

## Core objects
`Document` is a transport envelope, not a universal AST. It carries an IR identifier, media type, encoding, bytes, metadata, and an optional immutable AST tree. Plaintext uses bytes; Markdown carries its decoded source and a parsed MDAST tree. Format-specific parsers expose structured types rather than forcing every representation into a string blob.

`Result<T>` communicates either a value or a structured `Error`, plus successful diagnostics such as lossiness reports. Avoid exceptions as the normal control path for expected parse, I/O, and unsupported-conversion failures. Exceptions may still be caught at application boundaries for unexpected failures.

## Suggested conversion pipeline

```text
input bytes -> Reader -> Parser -> source IR -> Translator -> target IR
            -> Serializer/Writer -> output bytes
```

A driver can compose stages. The translator registry should key conversions by source IR and target IR, not only file extensions. Format sniffing should be advisory; explicit user options should take precedence.

The initial driver implements this pipeline for `plaintext`: `parse_document` validates/decodes text into `Plaintext`, an instance-owned `ConversionRegistry` applies an explicitly registered direct route, and `serialize_document` writes `plaintext`, `json-dump`, or `xml-dump`. All three output formats use the same IR; dumps do not manufacture a JSON AST or XMLInfoset. No format sniffing or route chaining is implemented.
The Markdown subset reader produces a partial MDAST tree. Registered routes either flatten it to Plaintext or emit `DoxTkJSON`/`DoxTkXML` serialized inspection envelopes. These are not generic JSON/XML ASTs; see the [Markdown manual](manual/markdown.md).

## Lossiness and errors
- Unsupported features and incompatible target models are rejected.
- Future permissive readers may preserve unknown nodes as opaque extensions when the format supports it.
- Lossy conversion must be opt-in and report what was dropped or approximated.
- Diagnostics should include format/IR, source location when available, and actionable context.

Markdown-to-Plaintext requires `allow_lossy` and reports the lost structure in `Result::diagnostics`. Flags do not enable unsupported built-in formats or character replacement. Plaintext serialization rejects metadata rather than dropping it. XML dumps reject XML 1.0-incompatible characters rather than filtering them.

## Dependency boundaries
The core target is dependency-free. Optional adapters can use Lua/Kaguya, libharu, pugixml, yaml-cpp, fmt/spdlog, or other third-party libraries behind explicit build options. Keep their types out of core public headers.
