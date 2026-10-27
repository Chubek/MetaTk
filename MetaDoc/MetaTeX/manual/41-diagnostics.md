# 41 — Diagnostics and resource limits

Document diagnostics carry severity, message, filename, line, and column.
The batch CLI prints them to stderr and prevents output writing when any are
errors. Warnings allow output. Exceptions, including resource exhaustion and
invalid source encoding, are handled separately by the CLI or embedding host.

Important current bounds include:

| Resource | Bound |
|---|---|
| Source bytes per ordinary read/scan | 16 MiB |
| Source tokens, queued tokens, processing operations | 1,500,000 limits |
| Nested token execution | 128 |
| Nested expansion/numeric scanning | 128 |
| Group scopes | 1024 including the base scope |
| Conditional nesting | 1024 |
| Input-file nesting | 16 |
| Captured scripting output | 4 MiB |

These limits protect different resources. A recursive macro may hit the
operation budget even if its token queue stays small. A large literal source
may hit the byte limit before any tokens execute.

Treat locations as scanner diagnostics, not a full macro-expansion backtrace:
tokens can retain their definition-time positions, and full expansion-context
source spans are future work. After an error, a REPL submission may retain
earlier successful definitions. Preserve a minimal reproducer, record the
format and target, and compare JSON structure with rendered output to localize
a defect. Tests use explicit expected diagnostics where recovery behavior
matters; a thrown exception alone is not proof of graceful recovery.

[Contents](README.md)
