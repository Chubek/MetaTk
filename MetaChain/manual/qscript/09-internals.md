# 9. Internals

## 9.1 Pipeline

```
source ──► grammar (ParseTk, scannerless) ──► match tree ──► AST (OPP for operators)
       ──► analysis: `use` loading, constants, names, arity, scopes
       ──► lowering to MetaIL ──► optimizer ──► block scheduler
       ──► MetaVM interpreter │ MetaJIT │ x86-64 assembly ──► ASMtk ──► LDtk ──► ELF
```

| Stage | Source | API |
|-------|--------|-----|
| Grammar engine | `qscript/include/QScript-Grammar.h` | `qscript::grammar::Grammar` |
| QScript grammar | `qscript/grammar/QScript.grammar` (embedded) | `qscript::language_grammar()` |
| AST builder | `qscript/src/AST.cpp` | `qscript::parse` |
| Analysis | `qscript/src/SemaCheck.cpp`, `Analysis.h` | `qscript::sema_check` |
| Lowering | `qscript/src/AST2IR.cpp` | `qscript::lower`, `ast_to_ir` |
| Optimize / schedule | `Optimizer.cpp`, `ISched.cpp` | `qscript::finish`, `optimize`, `schedule` |
| Execution, host calls | `Compiler.cpp` | `qscript::run`, `qscript::call` |
| Native | `ISel.cpp`, `Assembler.cpp`, `Linker.cpp` | `select`, `assemble`, `link`, `build` |
| C ABI | `QScript.cpp`, `src/Grammar.cpp`, `QScript.c` | `qs_*` |
| Embedding | `qscript/cmake/Embed.cmake` | generates `qscript_embedded.cpp` |

## 9.2 The AST

`qscript::AST` wraps a `dsl::ASTNode`. Statement, declaration and call
nodes may end with a location leaf `(@ "line:column:file")`; it is optional, so
hand-built ASTs can omit it.

| Node | Children |
|------|----------|
| `program`, `block` | items / statements |
| `use` | `(name LIB)...` |
| `fn` | `(name F) (params (name P)...) (block ...)` |
| `const` | `(name C) EXPR` |
| `let`, `assign` | `(name X) EXPR` (compound assignments and `++` are desugared to `assign`) |
| `if` | `COND THEN [ELSE]` (`unless`, `elsif` and modifiers desugar to `if`) |
| `while` | `COND BODY` (`until` and `loop` desugar to `while`) |
| `do` | `(block ...) COND` (`do ... until` negates COND) |
| `for` | `INIT COND STEP BODY` (missing parts are empty blocks / `(integer 1)`) |
| `break`, `continue` | none |
| `return`, `exit`, `assert`, `expr` | `EXPR` |
| `print` | `EXPR...` |
| `integer` | leaf: decimal value |
| `name` | leaf: identifier, possibly `lib::name` |
| `call` | `(name F) ARG...` |
| `neg`, `not`, `invert` | `EXPR` |
| `+ - * / % & \| ^ << >> == != < > <= >= && \|\| **` | `LHS RHS` |
| `?:` | `COND THEN ELSE` |

`qscript ast FILE` prints it.

## 9.3 Analysis

`detail::analyze` builds one `Analysis` for the whole program:

1. Items of the program are registered into unit `""`; each `use` loads a
   library (memoized, so cycles are fine) into a unit named after it, with
   qualified names `lib::f`.
2. If `**` appears in any function or statement, the hidden library `__core`
   (one function, `pow`) is loaded; `**` lowers to a call of `__core::pow`.
3. Every constant is evaluated, lazily and with cycle detection, using the
   same arithmetic as MetaIL (`metail::evaluate`).
4. The program body and every function body are checked: scopes, the
   16-variable limit, assignments, `break`/`continue` placement, calls and
   arity, and name resolution (chapter 4).
5. A call graph is built; the reachable set is the program plus every
   program-defined function plus everything they call. Only reachable
   functions are emitted.

## 9.4 Code generation and calling convention

| Registers | Use |
|-----------|-----|
| `r0`..`r15` | variables and parameters, allocated per block scope |
| `r16`..`r29` | expression temporaries (an expression may nest 14 deep) |
| `r30` | return value of the last call |
| `r31` | scratch: return address in prologues, immediate operands, assert traps |

Layout: the top-level statements first, ending `const r16, 0; halt r16`;
then each reachable function.

A call `f(a, b)` with result into temporary `t`:

```
push r<live variable>...        ; caller saves its live variables
push r16 .. r(t-1)              ; and temporaries holding partial results
<a into rt>; push rt            ; arguments, left to right
<b into rt>; push rt
call f
pop  ...                        ; restore in reverse
mov  rt, r30
```

The callee:

```
f:  pop  r31                    ; return address
    pop  r1                     ; parameters, last first
    pop  r0
    push r31
    ...body...
    mov  r30, r16 ; ret         ; for `return EXPR`
    const r30, 0  ; ret         ; falling off the end
```

All of this uses existing MetaIL instructions (`push`, `pop`, `call`, `ret`),
so the verifier, optimizer, scheduler, interpreter, JIT and x86-64 lowering
needed no changes. Host calls (`qscript::call`) relocate the program by one
instruction and enter through a stub that pushes the arguments, calls the
function and halts with `r30`.

`assert E` compiles to `jnz E, ok; const r31, 0; div r31, r31, r31; ok:`.
The division traps; `qscript::run` recognizes that exact instruction pair and
reports "assertion failed" with the statement's source location.

Short-circuit operators and `?:` compile to branches; `!=`, `<=`, `>=` and `>`
are built from `eq`/`lt` (MetaIL has no others).

## 9.5 Optimization and scheduling

The MetaIL optimizer does constant propagation inside basic blocks and
rewrites instructions in place; the scheduler reorders instructions inside
basic blocks of at most 256 instructions (larger blocks keep their order).
Neither changes instruction count or block boundaries, so function entry
points recorded during lowering stay valid. Calls and returns end blocks and
are barriers, as are `push`, `pop`, `print` and `halt`.

## 9.6 Source locations

Every emitted instruction carries the source location of the statement (or
call) it came from in the program's `source_map`. VM traps are reported at
that location; `qscript run` prints them as `file:line:column: message`.
Library code is reported with its library file name (`stdlib/math.qs`).

## 9.7 Limits

| Limit | Default | Where |
|-------|---------|-------|
| source size | 64 MiB | `toolchain.il.max_source_bytes` |
| AST nodes | 100,000 per source | `max_nodes` |
| syntax / AST depth | 256 | `max_depth` (cannot be raised) |
| grammar evaluation depth | 4096 | `grammar::ParseOptions::max_depth` |
| grammar operations | 2,000,000 + 512 per byte | `grammar::ParseOptions::max_operations` |
| live variables per function | 16 | fixed (MetaIL registers) |
| parameters per function | 16 | fixed |
| expression temporaries | 14 | fixed; split long right-nested expressions with `let` |
| libraries per program | 256 | `max_libraries` |
| program size | 1,000,000 instructions | `toolchain.il.max_instructions` |
| execution steps | 10,000,000 | `toolchain.vm.max_steps` |
| VM stack | 1,000,000 values | `toolchain.vm.max_stack_values` |

Every limit is checked and reported as a diagnostic; none can crash the
host.

## 9.8 Changes to ParseTk

Two additive changes were made to `metachain/ParseTk.hpp` for QScript:

* **Deepest-failure reporting.** `ParseResult` gained `expected`,
  `expected_span` and `expected_all`: the furthest terminal failures seen
  anywhere, including inside repetitions and optionals that later succeeded
  with a shorter match. Lookahead internals are excluded. The existing
  `error`/`error_span` fields behave as before.
* **Speed.** Failure messages are formatted lazily, references are bound to
  their symbols once per parse instead of hashed on every visit, and regex
  lexemes that are plain character classes (`[...]`, `[^...]`,
  `(?![...])[...]`, each optionally `(?:...)+`) are matched with a 256-entry
  byte table instead of `std::regex`. `EventStream::emit_all` delivers a
  batch with one listener snapshot.

## 9.9 Tests

| Test | Covers |
|------|--------|
| `tests/test_qscript.cpp` | language semantics, diagnostics, libraries, host calls, schedulers, JIT, grammar engine |
| `tests/qscript_c.c` | the whole C ABI, compiled as C |
| `tests/qscript_integration.py` | CLI commands, examples, every stdlib test program, `-L`, native executables, REPL |
| `tests/qscript-stdlib/*.qs` | one self-checking program per standard library |

```sh
ctest --test-dir build -R qscript --output-on-failure
cmake -S . -B build-asan -DMETACHAIN_SANITIZERS=ON && cmake --build build-asan && ctest --test-dir build-asan
```
