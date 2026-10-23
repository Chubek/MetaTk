# MetaML diagnostic codes

Canonical code registry for the current runtime. The table is deliberately
regular Markdown (`code`, `stage`, `meaning`) for tooling to consume. Messages
include source-dependent details; clients should inspect codes and spans.

| Code | Stage | Meaning |
| --- | --- | --- |
| M0101 | lexer | Unexpected source character |
| M0102 | lexer | Unterminated string literal |
| M0103 | lexer | Invalid string or Unicode escape |
| M0104 | lexer | Unterminated nested block comment |
| M0105 | lexer | Invalid/out-of-range numeric literal or placeholder above 1024 |
| M0201 | binding | Mixed parameter styles or invalid tacit index |
| M0202 | parser | Unexpected token or incomplete syntax |
| M0204 | binding | Unbound variable or operator |
| M0205 | parser | Invalid pattern syntax |
| M0206 | binding | Unbound/out-of-context tacit parameter |
| M0207 | parser | Section hole outside a section or invalid hole index |
| M0208 | binding | Duplicate label, binding, parameter, or constructor |
| M0209 | binding | Unknown constructor or module used as a value |
| M0210 | types | Unknown/misapplied type constructor, variable, or recursive alias |
| M0301 | types | Incompatible types or application of a non-function |
| M0302 | types | Infinite type detected by occurs check |
| M0303 | types | Incompatible record row or missing field |
| M0304 | types | Constructor pattern arity mismatch |
| M0305 | types | Definition does not satisfy its signature |
| M0306 | types | Numeric constraint violation |
| M0307 | warning | Unused function parameter |
| M0309 | types | Incompatible rigid type variable |
| M0400 | context | Directive in a non-top-level expression or structure |
| M0401 | evaluator | Non-exhaustive match or parameter/binding pattern mismatch |
| M0402 | evaluator | Integer division by zero |
| M0403 | evaluator | Primitive denied by the allowlist |
| M0404 | evaluator | Invalid runtime operation, primitive declaration, or allocation request |
| M0405 | evaluator | Unsupported structural comparison |
| M0406 | evaluator | Function/expression depth limit exceeded |
| M0407 | evaluator | Explicit Core.error |
| M0408 | evaluator | Global binding used before definition |
| M0409 | host | Recoverable native primitive exception, with primitive name and message |
| M0501 | modules | Unbound module path |
| M0502 | modules | Signature/ascription mismatch or missing implementation |
| M0503 | modules | Unknown or invalid functor |
| M0504 | modules | Unknown named signature |
| M0505 | loader | Missing/unreadable import or standard library, or invalid import context |
| M0506 | modules | Missing module member |
| M0507 | modules | Requested field unavailable during lazy initialization |
| M0601 | REPL | Unknown directive |
| M0602 | REPL | Reload requested before a load |
| M0603 | REPL | Missing/unreadable directive file argument |

`M0409` preserves the earlier use-before-definition meaning of `M0408`.
Standard C++ exceptions and recoverable D Exceptions raised by invoked host
primitives are converted; native faults, D Defects, and process termination
are not recoverable language failures. A primitive's existing MetaError keeps
its original diagnostic code.
