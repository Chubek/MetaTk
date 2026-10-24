# Chapter 29 — Resource limits

## Default budgets

`mair::Limits` contains four independent fields:

| Field | Default | Applied to |
| --- | --- | --- |
| `source_bytes` | 16777216 | Source text parsing and bounded file reads |
| `statements` | 1000000 | Parsed/emitted Program statement count |
| `nesting` | 64 | Operand splitting during parsing |
| `output_bytes` | 16777216 | Rendered text or total assembled section bytes |

```cpp
mair::Limits limits;
limits.source_bytes = 1024 * 1024;
limits.statements = 10000;
limits.output_bytes = 2 * 1024 * 1024;
auto p = mair::parse("encode ret\n", "small.mair", limits);
auto t = mair::target("amd64", "MetaArchDB");
auto image = mair::assemble(p, t, limits);
```

Passing the limits at both parsing and emission is intentional. A Program built
directly in C++ has not passed through parser source-size checks.

## Important distinctions

Native builder operands retain a nesting ceiling of 64 when `emit` validates
them. Raising the parser's `nesting` field does not remove that builder check.
Lowering it can reject excessive nesting earlier in textual input.

For `assemble`, the output budget counts section bytes and padding, not every
allocation used by symbol strings, statement containers or relocation metadata.
For `render`, it counts text, including escape expansion and the target header.
Limits are useful bounds, not a process-wide memory quota.

The CLI's `--max-bytes N` sets both source and output budgets. It additionally
checks the serialized result size: a hex string is approximately twice as large
as its binary bytes, and JSON adds metadata. The CLI does not expose separate
statement or nesting flags.

## Separate database limits

These limits apply to assembly source and output. Target specification loading
uses the MetaArch loader's own options, and individual encoder execution has
the underlying VM's limits. Increasing a MetaArchIR output budget does not
increase an encoder's maximum instruction-emission capacity.

[Previous](28-files-and-pipelines.md) · [Contents](README.md) · [Next](30-riscv-walkthrough.md)
