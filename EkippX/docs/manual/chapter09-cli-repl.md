# Chapter 9 - CLI and REPL

The command line executable supports batch expansion, evaluation, tracing, symbol dumps, and an interactive PikoRL-backed REPL.

## Purpose

- Chapter 9 - CLI and REPL treats `purpose` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Operational Model

- Chapter 9 - CLI and REPL treats `operational-model` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Interfaces

- Chapter 9 - CLI and REPL treats `interfaces` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Diagnostics

- Chapter 9 - CLI and REPL treats `diagnostics` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Build Integration

- Chapter 9 - CLI and REPL treats `build-integration` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Extension Points

- Chapter 9 - CLI and REPL treats `extension-points` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Safety Rules

- Chapter 9 - CLI and REPL treats `safety-rules` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Examples

- Chapter 9 - CLI and REPL treats `examples` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Failure Modes

- Chapter 9 - CLI and REPL treats `failure-modes` as part of the public documentation contract.
- Prefer deterministic expansion, explicit options, and reproducible diagnostics over implicit host state.
- Keep examples small enough to compare with a golden output file during smoke testing.
- Record ownership: the caller owns input strings, the context owns registered symbols, and plugins own their private state.
- Preserve source names and line numbers whenever an operation can fail.
- Avoid hidden filesystem access unless the include or plugin policy explicitly permits it.
- Use the same invocation syntax in manuals, tests, examples, and CLI help text.

## Review Checklist

- Chapter 9 - CLI and REPL treats `review-checklist` as part of the public documentation contract.
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
