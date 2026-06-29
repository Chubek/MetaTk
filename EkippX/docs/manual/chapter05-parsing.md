# Chapter 5 - Parsing and Syntax

Parsing canonicalizes invocations while preserving source positions for diagnostics and trace reporting.

## Purpose

- Chapter 5 - Parsing and Syntax treats `purpose` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Operational Model

- Chapter 5 - Parsing and Syntax treats `operational-model` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Interfaces

- Chapter 5 - Parsing and Syntax treats `interfaces` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Diagnostics

- Chapter 5 - Parsing and Syntax treats `diagnostics` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Build Integration

- Chapter 5 - Parsing and Syntax treats `build-integration` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Extension Points

- Chapter 5 - Parsing and Syntax treats `extension-points` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Safety Rules

- Chapter 5 - Parsing and Syntax treats `safety-rules` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Examples

- Chapter 5 - Parsing and Syntax treats `examples` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Failure Modes

- Chapter 5 - Parsing and Syntax treats `failure-modes` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Review Checklist

- Chapter 5 - Parsing and Syntax treats `review-checklist` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Minimal Smoke Pattern

```text
@define(NAME, EkippX)
@emitln(&upper(NAME))
```

- Expected output contains `EKIPPX` followed by the configured newline mode.
