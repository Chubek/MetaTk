# 23. Diagnostics, failure, and recovery

Language failures are structured DSLtk diagnostics. Each carries a severity, code, message, and source span, with optional additional context. `Session.render` combines a diagnostic with remembered source text to produce a readable caret display.

| Family | Area |
| --- | --- |
| `M01xx` | Lexing and malformed literals |
| `M02xx` | Parsing, binding syntax, placeholder context |
| `M03xx` | Type checking |
| `M04xx` | Evaluation and primitive invocation |
| `M05xx` | Modules and imports |
| `M06xx` | REPL and file directives |

`M0401` denotes a failed pattern, `M0402` integer division by zero, `M0403` a denied primitive, `M0404` an invalid runtime operation, `M0405` an unsupported comparison, `M0406` depth exhaustion, and `M0407` explicit `Core.error`. `M0408` reports use before definition. `M0409` wraps a recoverable host callback exception, including its primitive name and message. Oversized numbered placeholders and invalid numeric separators use `M0105`. The canonical code list is in [`manual/errors.md`](../manual/errors.md).

Ordinary `Result` and `Option` values are not diagnostics. They are data that a caller can match and transform. Prefer them for expected failures such as parsing a user-entered integer or looking up a missing table key. Use `Core.error` for a violated invariant that should abort the current computation.

```metaml
parse_count s := match Int.from_string s with
  | Some n when n >= 0 -> Ok n
  | _ -> Err "expected a non-negative count"
Test.equal "expected failure" (parse_count "oops") (Err "expected a non-negative count")
```

On an evaluation diagnostic, the session resets interpreter depth bookkeeping and its observable value stack. The session remains useful for later submissions, but definitions and effects from earlier units can remain. Host process faults, invalid native pointers, allocation exhaustion, and infinite loops are not made harmless by diagnostic conversion.

For machine clients, inspect codes and spans rather than parsing rendered English text.

<!-- nav:begin -->
[Contents](README.md) · [Previous](22-imports.md) · [Next](24-core-and-prelude.md)
<!-- nav:end -->
