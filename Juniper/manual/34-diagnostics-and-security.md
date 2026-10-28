# 34. Diagnostics, limits, and secure operation

Good error handling distinguishes source failures, runtime deployment mistakes,
unsupported backend coverage, and actual file I/O errors. Do not suppress a
diagnostic and publish a half-formed artifact.

## Diagnose the stage, not just the message

| Code | Boundary to inspect |
|---|---|
| `JUN-E100` | MetaIL parsing, validation, or pass operation |
| `JUN-E101` | Juniper version/backend metadata syntax |
| `JUN-E102` | Invalid backend resource/rule/profile |
| `JUN-E103` | Invalid IR kind, NUL, or structural budget |
| `JUN-E104` | Invalid API argument or moved-from context |
| `JUN-E200` | Process startup, pipes, or missing runtime resource |
| `JUN-E201` | GPPX failure or diagnostic output, including warnings |
| `JUN-E202` | GPPX timeout |
| `JUN-E203` | GPPX output limit |
| `JUN-E204` | Invalid preprocessing limits/arguments |
| `JUN-E205` | Unreadable root input |
| `JUN-E300` | No legal selection or selection budget |
| `JUN-E400` / `JUN-E401` | Invalid machine IR/dependency scheduling |
| `JUN-E500` | Explicit MetaArch form mapping/encoding |

Codes are more stable than human-readable text. Metadata diagnostics include
line/column information; MetaIL failures retain its diagnostic message. Expanded
IR coordinates are not a full macro-source expansion backtrace. Start difficult
macro investigations by saving `preprocess` output.

## A controlled negative test

<!-- check: juniper error=JUN-E101 -->
```juniper
backend impossible {
  issue 0;
}
fn identity(x) {
  return x;
}
```

This is intentionally invalid. The parser reports a metadata failure for an
out-of-range issue width; do not build application logic around the exact text.
The manual checker checks the actual code family for this example.

## Important fixed and configurable bounds

| Area | Current boundary |
|---|---|
| IR source / native IR strings | 8 MiB respective budget |
| IR structure | 200000 items; expression depth 256 |
| MetaIL source tokens | 200000 |
| Backend | 1024 rules; 64 resources |
| Patterns | depth 32; 16384 aggregate nodes |
| Issue/resource width | 1 through 64 |
| Rule cost/latency | 1 through 1000000 |
| Selection | 200000 instructions per function; 20000000 pattern-node attempts |
| GPPX default | 10 seconds; 8 MiB stdout; 64 KiB stderr |

The C++ preprocessing API can change its output/time budget within documented
ranges. It does not change the subsequent parser's fixed limits. On the CLI,
`--timeout N` changes the GPPX deadline; it is not a timeout for all MetaIL analysis
and optimization work.

## Treat macros and resources as trusted input

The driver does not invoke a shell and leaves `#exec` disabled. Include reads are
still permitted, executable/runtime paths can select arbitrary programs/files,
and the process inherits an environment. Use filesystem/process isolation for
untrusted inputs; the existing limits are not a sandbox. Do not load unknown
runtime/kernel directories simply because an IR file is untrusted.

The CLI refuses output/depfile aliases of known input dependencies, including
hardlink equivalence. But shell redirection can truncate a file before Juniper
starts, and concurrent hostile filesystem mutation is outside this protection.
Use explicit `-o`, separate output directories, and controlled build inputs.

## Troubleshooting sequence

1. Check `--version` and environment/resource overrides.
2. Run `preprocess`, then `check`, then `emit` as separate stages.
3. If `check` passes but `select` fails, inspect rule coverage and optimization shape.
4. If scheduling fails, inspect IDs, resources, latencies, operands, and barriers.
5. If encoding fails, inspect the form mapping and physical assignments first.

Runtime initialization can throw before a Result exists in C++; keep the outer
exception boundary shown in chapter 20. For automation, use the CLI's documented
exit statuses rather than treating every nonzero status as malformed source.

[Previous](33-cmake.md) · [Index](README.md) · [Next](35-testing.md)
