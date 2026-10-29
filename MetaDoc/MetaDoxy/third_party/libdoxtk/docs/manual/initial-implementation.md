# Initial implementation

This page describes the first Plaintext baseline. The [Markdown subset](markdown.md) page documents the subsequent AST work and current conversion policy.

## Supported capabilities

| Format name | IR | Read | Write |
| --- | --- | --- | --- |
| `plaintext` | `Plaintext` | yes | yes |
| `json-dump` | `Plaintext` | no | yes |
| `xml-dump` | `Plaintext` | no | yes |

Plaintext is one Unicode text value carried in `Document::data` as UTF-8. Parsing preserves empty content, embedded NULs, BOMs, whitespace, and line endings. It performs no markup recognition, newline normalization, or encoding detection. Supported input encodings are exactly `utf-8`, `ascii`, and `iso-8859-1`; output is UTF-8. Latin-1 is not Windows-1252.

Malformed UTF-8 is rejected with a zero-based byte offset, including truncated/overlong sequences, surrogate encodings, stray continuation bytes, and values above U+10FFFF. ASCII rejects bytes above 127. No decoding mode inserts replacement characters.

## Library API

```cpp
#include <DoxTk.hpp>
#include <doxtk/DoxTk-Bytestream.hpp>

doxtk::ConvertOptions options;
options.source_format = "plaintext";
options.target_format = "json-dump";
const auto output = doxtk::convert(doxtk::to_bytes("hello\r\n"), options);
if (!output) {
    // Inspect output.error.code, .message, and .context.
}
```

`parse_document`, `ConversionRegistry::translate`, and `serialize_document` can also be composed independently. `builtin_formats()` reports immutable capabilities; `builtin_conversions()` returns a fresh registry containing only `Plaintext -> Plaintext`. Registration requires nonempty IR names and a callback; duplicate routes are rejected. Translation does not search for multi-hop routes and checks that a successful callback returns the requested IR.

Registries have no global mutable state. Finish registration before sharing a registry between threads; callbacks must be safe for concurrent invocation. Callbacks receive `ConvertOptions` and are responsible for their own lossiness/error policy. Built-ins are lossless regardless of `strict`/`allow_lossy`: these flags do not enable unimplemented features.

`read_file` and `write_file` are binary-safe and report open/read/write/close failures. `convert_file` finishes parsing and serialization before opening output, so malformed input and unsupported conversions do not truncate an existing output. In-place conversion is supported because input is fully read first. Writes are **not atomic**: an I/O failure after opening output can leave it partial. All operations currently buffer the entire document in memory.

## Dump contracts

JSON uses a single object with fields in this fixed order: `ir`, `media_type`, `encoding`, `text`, `metadata`. Metadata keys are sorted lexicographically. Controls, quotes, and backslashes are escaped; other Unicode text is emitted as UTF-8. Output ends with a newline.

XML uses a UTF-8 XML 1.0 declaration and a `document` element with `ir`, `media_type`, and `encoding` attributes, followed by `text` and `metadata`. Each metadata item is an `entry` with a `key` attribute. Special characters are escaped; CR/LF/tab use numeric references to preserve text and attribute whitespace across XML parsing. NUL, forbidden controls, U+FFFE, and U+FFFF are rejected with `serialization_error`; use JSON for such text. Metadata and media-type strings must themselves be valid UTF-8.

Both dumps contain the **decoded** text and declare UTF-8, not the original source encoding or byte stream. They are inspection formats, not persistence/round-trip APIs: no dump deserializer is implemented. Plaintext output refuses documents with metadata because it cannot represent that metadata.

## Command-line tools

```sh
doxtk-convert --list-formats
doxtk-convert --from plaintext --to plaintext input.txt output.txt
doxtk-convert --encoding iso-8859-1 --to json-dump input.txt
dump2json input.txt output.json
dump2xml input.txt output.xml
```

Input defaults to `plaintext` and `utf-8`. `doxtk-convert` requires `--to`; dump tools have fixed output formats. Omitting OUTPUT writes to standard output; INPUT must be a file (stdin is not implemented). `--` ends option parsing, allowing filenames beginning with `-`. Use `--help`, `--version`, or `--list-formats` for inspection. Exit codes are 0 for success, 1 for conversion/I/O failures, and 2 for argument errors. Diagnostics go to standard error.

The legacy PDF, DVI, PostScript, and XTAS tool names still provide scaffold help and fail clearly on conversion. They do not claim to convert arbitrary input.

## Installation and remaining work

`cmake --install build --prefix PREFIX` installs headers, the library, and its CMake package. Consumers use `find_package(DoxTk CONFIG REQUIRED)` and link `DoxTk::DoxTk`. CLI executables currently remain in the build tree.

The umbrella `DoxTk.hpp` exposes the core API without optional dependencies. The subsequent [C++ native DSL](cppdsl.md) uses DSLtk behind the optional `DoxTk::CPPDSL` interface target.

Full Markdown/HTML/AsciiDoc parsing, Doxir compilation/runtime validation, Lua/SWIG adapters, PDF output, XML parsing, YAML parsing, bytecode, event streaming, and IPC remain unimplemented. Partial Markdown parsing, ASTs, and traversal are described in the Markdown manual. Optional integration switches fail with actionable configuration errors. IR definition tables are descriptive metadata, not executable canonical Doxir schemas.
