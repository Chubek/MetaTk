# Juniper language, version 1

## Macro layer

The hard-coded GPPX profile uses `@name(args)` for macros and `#directive` for
directives. Numbered arguments are `#1` through `#9`; parentheses protect commas.
`#define @name body`, `#include "file"`, `#if`, `#ifdef name`, `#else`, `#endif`,
`#eval` and other upstream GPP directives retain their meanings. `#ifdef` and
`#ifndef` test bare identifiers (for example, `#ifdef FEATURE`). Quotes protect
string contents. `//` and `/* ... */` comments are stripped by preprocessing.
There is no execution flag and `#exec` fails.

Includes are searched deterministically in: **input root directory**, standard
library directory, then explicit `-I` directories. No working-directory or system
include search occurs. Nested includes are root/include-path-relative, not
includer-relative. GPPX inclusion markers are retained as comments in expanded
output and used to record transitive dependencies. Parser diagnostics refer to
expanded IR coordinates; they are not a full macro expansion backtrace.

Standard macros:

| Macro | Expansion |
|---|---|
| `@module(name)` | `module name;` |
| `@fn(name, (p1,p2))` | `fn name (p1,p2) {` |
| `@end` | `}` |
| `@let(name,value)` | `let name = value;` |
| `@return(value)`, `@eval(value)`, `@assume(value)` | corresponding statement |
| `@label(name)`, `@goto(name)`, `@br(condition,yes,no)` | corresponding CFG statement |
| `@version` | `juniper 1;` |
| `@backend(name)` | `backend name extends portable {` |
| `@issue(n)`, `@resource(name,n)` | scheduling declaration |
| `@rule(name,pattern,opcode,cost,latency,resource)` | selection rule |

Empty parameter lists use `@fn(main, ())`. Mix macros with ordinary MetaIL freely.
Define feature macros using `-DFEATURE=1` and reference them as `@FEATURE`.

## Canonical IR

```ebnf
file     = ["juniper", "1", ";"], [backend], metail_module;
backend  = "backend", identifier, ["extends", "portable"], "{", {entry}, "}";
entry    = "issue", positive_integer, ";"
         | "resource", identifier, positive_integer, ";"
         | "rule", identifier, ":", pattern, "=>", identifier, {attribute}, ";";
attribute = "cost", positive_integer | "latency", positive_integer
          | "resource", identifier;
pattern  = "reg" | "imm", ["(", integer, ",", integer, ")"]
         | operator, "(", pattern, {",", pattern}, ")";
```

Backend and version declarations must precede the MetaIL module/functions.
`extends portable` copies the portable rules before adding local declarations.
Without a backend declaration the portable backend is used. A backend without
`extends` has no selection rules, but starts with `alu` and `control` capacity 1.
Local resource declarations may replace inherited capacities; duplicate local
resources, issue settings, rule names or rule attributes are errors. Rules default
to cost 1, latency 1 and resource `alu`. First declaration wins equal-cost ties.
Canonical emission writes the fully expanded backend, without inheritance.

Identifiers use `[A-Za-z_][A-Za-z0-9_]*`. Metadata integers are signed-64 decimal;
the MetaIL body retains its own decimal/hexadecimal literal syntax. Rule operators
must have MetaIL's canonical arity. Lazy `select` cannot appear in a rule. Possible
traps (`div`, `mod`, `shl`, `shr`) can only appear at a rule's root.

## Program semantics

The body is the actual MetaIL language: optional `module name;`, then one or more
`fn name(parameters) { statements }`. Statements are mutable `let`, `return`,
`eval`, `assume`, labels, `goto`, and `br condition,yes,no;`. Expressions are
signed-64 literals, locals, calls, and canonical unary/binary operators or their
infix spelling. `select(condition,yes,no)` evaluates only the chosen arm; logical
`land`/`lor` evaluate **both** operands. Calls are conservatively effectful.

Addition, subtraction, negation and multiplication wrap modulo 2^64. Division
and remainder truncate toward zero; zero divisors and signed-minimum / -1 are
potential traps. Shift counts outside 0..63 are potential traps. `shr` is signed.
The Juniper frontend never folds these independently; MetaIL's real kernels do
the optimization under the canonical effect/trap contract.

Validation checks structure and CFG targets, **not** definite assignment, call
resolution or termination. Variables are mutable, not SSA values. Functions and
modules own their contents; no borrowed MetaIL objects escape public methods.

## Limits and diagnostics

Input: 8 MiB; IR: 200000 items and 256 expression depth; backend: 1024 rules,
64 resources, 16384 pattern nodes and depth 32; issue/resource widths: 1..64;
cost/latency: 1..1000000; selection: 200000 instructions per function and
20000000 pattern-node matches. GPPX defaults: 10 seconds, 8 MiB output and
64 KiB diagnostic output. GPPX warnings are errors, not silently discarded.

Stable diagnostic families: `JUN-E100` MetaIL; `E101` metadata syntax; `E102`
backend validation; `E103` IR budgets; `E104` API arguments/context; `E200..205`
process/input/expansion; `E300` selection; `E400..401` scheduling; `E500` MetaArch.
Text can evolve; codes are not renumbered.
