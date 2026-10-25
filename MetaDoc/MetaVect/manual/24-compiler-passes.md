# Chapter 24 — Compiler Passes

**Status:** scaffold contracts (`compiler/CalcOpt.d`,
`compiler/Memoize.d`, `compiler/MVIR-Term.d`).

## Orchestration (`CalcOpt`)

Pass order is explicit data, and every pass can be disabled for debugging.
The intended order runs Mandatory cleanup first, independent local passes
in a fixed sequence, then optional profile-guided and target-specific
passes last — deterministic regardless of host parallelism.

## Memoization (`Memoize`)

Pure compiler computations may be cached, but cache keys must include
every semantic input: types, effects, target capabilities, and fast-math
flags alongside the term. A key missing any of these is a miscompilation
waiting for a second backend.

## Terms (`MVIR-Term`)

The typed term model — literals, variables, lambdas, applications, lets,
vector-graphics primitives — is the shared vocabulary of lowering
(chapter 22), rewriting (chapter 23), selection (chapter 25), and emission
(chapter 27). All four must agree on one term definition; that agreement
is this file's future job.
