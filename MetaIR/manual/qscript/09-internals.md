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
| Execution, host calls | `Compiler.cpp`, `Value.cpp`, `ValueAPI.cpp` | `run`, `call`, `call_value`, owning `Value`/`qs_value` |
| Native | `ISel.cpp`, `NativeRuntime.cpp`, `NativeBridge.cpp`, `NativeLink.cpp`, `qscript/native/`, `RuntimeTables.cpp`, `Assembler.cpp`, `Linker.cpp` | `select`, `assemble`, `link`, `build`, static parser ABI |
| Portable C99 | `CEmitter.cpp`, `CRuntime.cpp`, `GeneratedRuntime.cpp`, `metachain/CSource.hpp` | `emit_c`, `lower_c99`, `CRuntime`, linked `qs_runtime_*` bridge |
| C interoperability | `FFI.cpp`, `CFFI.cpp` | registered calls, serialization, loader, libffi |
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
| `fn` | `(name F) (params (param (name P) (type T))...) (type R) (block ...)` |
| `grammar` | `(name G) (name PARENT or empty) (grammar-body TEXT)` |
| `const` | `(name C) EXPR` |
| `let` | `(name X) (type T or empty for inferred) EXPR` |
| `assign` | `(name X) EXPR` (compound assignments and `++` are desugared to `assign`) |
| `index-assign` | `TARGET INDEX EXPR`; `TARGET` is a name or an `index` node, so `g[i][j] = v` nests |
| `index-update` | `TARGET INDEX OP EXPR`; `x[i] += v` and `x[i]++` (with `OP` the binary operator) |
| `foreach` | `(name X) EXPR BLOCK` (`for (my X in EXPR)`) |
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
| `method` | `RECEIVER (name M) ARG...` |
| `index` | `BASE INDEX` |
| `list` | `ELEM...` |
| `string` | leaf: the literal's bytes |
| `grammar-lit` | `(grammar-body TEXT)` |
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

Script calls use MetaIL's `push`, `pop`, `call`, `ret`.
Calls from the host (`qscript::call`) relocate the program by one
instruction and enter through a stub that pushes the arguments, calls the
function and halts with `r30`.

Registered host functions get the same prologue and execute `host r30, ID`
followed by `ret`. MetaIL dispatches through `VMOptions::host_call`, passing
a snapshot of all 32 registers; QScript's binding passes only the declared
arguments. Host instructions are effect/trap barriers for scheduling and
invalidate optimizer constant knowledge. The interpreter and JIT both implement
them; syscall-only ELF lowering rejects them.

`assert E` compiles to `jnz E, ok; const r31, 0; div r31, r31, r31; ok:`.
The division traps; `qscript::run` recognizes that exact instruction pair and
reports "assertion failed" with the statement's source location.

Short-circuit operators and `?:` compile to branches; `!=`, `<=`, `>=` and `>`
are built from `eq`/`lt` (MetaIL has no others).

## 9.5 Optimization and scheduling

The MetaIL optimizer does constant propagation, integer identity simplification,
and known-condition branch folding inside basic blocks and
rewrites instructions in place; the scheduler reorders instructions inside
basic blocks of at most 256 instructions (larger blocks keep their order).
Neither changes instruction count, so function entry
points recorded during lowering stay valid. Calls and returns end blocks and
are barriers, as are `push`, `pop`, `print`, `host` and `halt`. Potentially
trapping arithmetic is retained unless it can be evaluated successfully.

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
| canonical host type depth | 256 including scalar | iterative `parse_type`; nested lists are bounded |
| copied host match depth/nodes | 256 / 100,000 | `Value::from_match`, `Options::max_depth` / `max_nodes` |
| grammar evaluation depth | 4096 | `grammar::ParseOptions::max_depth` |
| grammar operations | 2,000,000 + 512 per byte | `grammar::ParseOptions::max_operations` |
| live variables per function | 16 | fixed (MetaIL registers) |
| parameters per function | 16 | fixed |
| expression temporaries | 14 | fixed; split long right-nested expressions with `let` |
| libraries per program | 256 | `max_libraries` |
| program size | 1,000,000 instructions | `toolchain.il.max_instructions` |
| execution steps | 10,000,000 | `toolchain.vm.max_steps` |
| VM stack | 1,000,000 values | `toolchain.vm.max_stack_values` |
| native parser link timeout | 60 seconds, maximum 3600 | `native_link.timeout_seconds`; includes subprocess descendants |
| retained linker diagnostics | 1 MiB, maximum 16 MiB | `native_link.max_diagnostic_bytes`; extra output is drained/discarded |

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
| `tests/test_qscript_values.cpp`, `tests/qscript_values_c.c` | all typed host values on VM/JIT, ownership, alias preservation, invalid signatures/limits, generated-runtime C ABI |
| `tests/qscript_integration.py` | CLI commands, examples, every stdlib test program, `-L`, native executables, REPL |
| `tests/qscript-stdlib/*.qs` | one self-checking program per standard library |
| `tests/qscript_ffi_c.c`, `qscript_ffi_fixture.c` | real C ABI, native signatures, ownership, binary exchange |
| `tests/test_qscript_ffi.cpp` | eager JIT callbacks and optimizer differential tests |
| `tests/qscript_c_backend.py` | compile generated C and compare its results/traps with the VM |
| `tests/qscript_c_language.py` | all language/error programs compiled as C99; grammar/match programs link the runtime; no grammar refusal exceptions |
| `tests/test_qscript_native.cpp` | native grammar execution with no runtime paths, static ELF/W^X checks, C ABI emission, limits, quoted paths, invalid/noisy/timed-out linker processes and cleanup |
| `tests/qscript_native_language.py` | all language/error programs as native executables, including grammars; no default grammar refusal exceptions |
| `tests/qscript_language.py`, `tests/qscript-language/` | one program per language area (strings, lists, control, grammars, printing) with exact expected output; `errors/*.qs` with the expected diagnostic text |

```sh
ctest --test-dir build -R qscript --output-on-failure
cmake -S . -B build-asan -DMETACHAIN_SANITIZERS=ON && cmake --build build-asan && ctest --test-dir build-asan
```

## 9.10 Typed lowering and built-ins

Lowering (`qscript/src/AST2IR.cpp`) is typed. `infer` computes the type of an
expression without emitting anything; `expr(node, expected)` checks the type
and then emits code. The first error aborts lowering and is reported as one
`verify` diagnostic at the node's source position.

Built-in methods and global functions are resolved by `resolve_method` and
`resolve_global` (`qscript/src/Runtime.cpp`) into a `BuiltinCall`: the
operation, its parameter types (the receiver first for methods), its result
type, and, for element-dependent operations, a **type descriptor**. The
descriptor is passed as a final constant argument, an index into the unit's
type table, and `Session::type_of` decodes it. `push`, `sort`, `contains` and
the generic `print` use it.

Each built-in call is an ordinary call to a stub. The stub's name is
`"\x01"` followed by the built-in's id, which cannot be written in QScript
and so cannot clash with a program function. Its body is:

```
pop r31            ; return address
pop r(N-1) .. r0  ; parameters, last one first
push r31
host r30, ID      ; ID = 2^40 + Builtin
ret
```

The VM's `host` callback receives `r0..r31` and forwards built-in ids to
`Session::dispatch`. The result comes back in `r30`, like any call result.
String and grammar literals are created from the unit's tables the first time
a run uses them and reused after that; list literals are built each time they
are evaluated.

Printing has two passes. The first lowering emits `print` for integers, which
is the cheapest form. If the program prints anything else, it is lowered again
so that every `print` goes through the host, which keeps the output in order
(`Lowerer::prints_text`).

Function ends: an integer function that falls off its end returns `0`. For any
other result type, `SemaCheck.cpp` (`terminates`) rejects a body that can reach its
end, and the lowering still ends the function with a call to `die`, so a run that
somehow reaches the end stops with `function NAME ended without returning a value`.
The check is conservative: a statement whose exit cannot be shown does not count as
terminating.

`assert c` lowers to `const r31, 0` followed by `div r31, r31, r31`, which
traps. `Compiler.cpp` translates that trap into `assertion failed` by checking
the instruction pair, so an unrelated division by zero keeps its own message.

Exports: `Unit::functions` holds the program's own functions and the library
functions reachable from them (a walk over the call fixups from those roots). The
C API and `qscript call` can enter only those. `Unit::libraries` lists every
library the program loaded.

Programs using built-ins need their unit to be selected (`select(const Unit&)`), because
the native run-time's tables come from `run_time_types` (`RuntimeTables.cpp`) and
`native_runtime_text` (`NativeRuntime.cpp`). The two run-times share the
`runtime_builtins` table in `Runtime.h`.

`needs_full_runtime` checks both built-in IDs and nested type descriptors, so
even an empty printed `list<match>` selects the complete runtime. Native
`NativeBridge.cpp` supplies a SysV C-ABI entry/exit pair to the core lowering,
with frame-to-register-array and stack-alignment adapters. `qscript/native/`
provides a minimal static launcher and direct `Session::dispatch`; no compiler
or VM is linked. `RuntimeConfig.h` shares table validation with generated C.
The native object exports a versioned unit table and an entry symbol;
`NativeLink.cpp` recognizes them in validated ELF symbol records and supervises
the static C++ link. A successful output must be static `ET_EXEC`, have no
interpreter/dynamic segment and obey W^X/non-executable-stack constraints.
Integer/string/list-only native code keeps the old ASMtk/LDtk route unchanged.

The C backend runs the string and list built-ins through a run-time written in
C99 (`qscript/src/CRuntime.cpp`). `emit_c` (`CEmitter.cpp`) adds to the generated
program the tables that describe the unit (string literals, type descriptors,
built-in numbers, heap limits) and passes the run-time to
`metachain::lower_c99` as a `CRuntime`. The core generator has no QScript
knowledge: its only hooks are the prelude, the start and end statements, the
dispatch function for `Op::Host` and the standalone helpers. For grammar/match
operations or descriptors (including empty `list<grammar>`), the prelude instead
uses `QScript-Generated.h`/`GeneratedRuntime.cpp`. This linked C ABI copies the
unit tables into an owning runtime and dispatches all built-ins to `Session`,
so there is one shared handle space for strings, lists, grammars and matches.
There is no VM fallback: generated C still implements the program's arithmetic,
control flow, step budget and operand stack. Unknown built-in IDs are rejected
at emission, rather than accepted through the bridge as arbitrary operations.

The C run-time repeats the semantics of `Runtime.cpp`, with the same messages
and the same heap accounting. Keep the two in step: a built-in changed in one
place must be changed in the other, and the C language runner
(`tests/qscript_c_language.py`) and the VM runner both have to pass.

Typed embedding (`Value.cpp`, `ValueAPI.cpp`): `Value` shares immutable host
storage, never a session handle. `Session::import_value` memoizes host identities
so repeated/nested arguments retain aliases within a call, while copying their
mutable lists into the session. `export_value` memoizes handle/type pairs and
copies list contents; grammar objects and match roots use shared ownership.
`Compiler.cpp::call_value` validates signatures, imports arguments, runs the
same verified/relocated function stub as integer calls and exports the result
before destroying the session. Failed executions retain statistics but do not
export handles. Canonical type parsing is iterative and bounded to 256 levels;
host-supplied match trees are checked iteratively before recursive copying.
