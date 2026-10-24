# Chapter 26 — The JSON image format

## Request an image

```sh
build/marchir -T amd64 --db MetaArchDB --emit json \
  MetaArchIR/examples/cli/amd64/call.mair -o call.json
```

JSON output is a versioned section-image representation. It is not the MetaArch
architecture schema, a serialization of the source Program, or an ELF object.
The current writer is part of the CLI; the header does not expose a public
`write_json` function or an image JSON reader.

## A minimal complete example

For the `return_to_caller` source in chapter 4, JSON has this structure; whitespace
here is expanded for readability:

```json
{
  "schema": 1,
  "target": "amd64",
  "target_index": 0,
  "sections": [{"name": ".text", "alignment": 1, "hex": "c3"}],
  "symbols": [{"name": "return_to_caller", "section": 0, "offset": 0, "global": true}],
  "relocations": []
}
```

`target` identifies the chosen database name and `target_index` its selected
target record. The document does not embed that database or its revision hash.
Consumers requiring reproducibility should track the database revision separately.

## Arrays and units

Sections contain their lowercase hexadecimal byte strings without separators.
An empty section has `hex: ""`. `alignment` is a byte alignment, not an exponent.
Symbols use section-vector indices and byte offsets.

Relocations carry `name`, `parameter`, `symbol`, `section`, `offset`, `size`,
`value`, `bit_offset`, `bit_width`, `byte_order` and `pc_relative`. `byte_order`
is the string `little` or `big`; offsets and sizes are bytes except for the two
explicit bit fields. An empty symbol string denotes a numeric relocation input.

## Consumer guidance

Check `schema` before interpreting records. Decode hex to bytes, preserve array
order and resolve relocation names against the same database semantics. Languages
whose JSON numeric type cannot represent all signed 64-bit values exactly need
an integer-preserving parser for large `value` fields. The CLI writes compact
JSON with a trailing newline, and its serialized size counts against `--max-bytes`.

[Previous](25-relocations.md) · [Contents](README.md) · [Next](27-cli-reference.md)
