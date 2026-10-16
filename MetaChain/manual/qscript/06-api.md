# 6. The C and C++ API

QScript has a C ABI (usable from C99 and any FFI) and a richer C++ API. Both
live in the same headers: the C declarations come first, the C++ part is
compiled only under `__cplusplus`.

| Header | Contents |
|--------|----------|
| `qscript/QScript.h` | contexts, compile, run, emit; the C++ pipeline (`qscript::compile`, `lower`, `run`, `call`, ...) |
| `qscript/include/QScript-Runtime.h` | engines, limits, run statistics, host calls, program introspection |
| `qscript/include/QScript-Library.h` | the embedded standard library and `use` resolution |
| `qscript/include/QScript-Module.h` | multi-source modules, module-local libraries |
| `qscript/include/QScript-Diagnostics.h` | structured diagnostics, checking, AST and match-tree dumps |
| `qscript/include/QScript-Backend.h` | scheduler and optimizer configuration, backend report |
| `qscript/include/QScript-Grammar.h` | the Raku-style grammar engine and match trees |

Link against `MetaChain::QScript` (`libQScript.a`).

## 6.1 Conventions

* Every function that can fail returns `qs_status`: `QS_OK` (0), `QS_ERROR`
  (1, see `qs_error`) or `QS_INVALID_ARGUMENT` (2, a NULL or out-of-range
  argument; the context is not modified).
* A `qs_context` holds configuration and the result of its **last
  operation**: `qs_error()` text, structured diagnostics, and any returned
  buffer (`const char **text` outputs). These stay valid until the next
  operation on the same context. Each operation clears them first.
* Handles (`qs_program`, `qs_module`, `qs_grammar`, root `qs_match`) are owned
  by the caller and released with their `_destroy` function; destroying NULL
  is a no-op.
* A context is not thread-safe; use one context per thread. Programs and
  grammars are immutable after creation and may be shared between contexts
  for reading.
* No function writes to the filesystem. Only `qs_module_add_file` and `use`
  with a library path read from it.
* C++ functions return `qscript::Result<T>` (= `metachain::Outcome<T>`): a value
  or a list of `metachain::Diagnostic {stage, message, file, line, column, offset}`.
  They never throw for invalid input.

## 6.2 Core: `qscript/QScript.h`

```c
const char *qs_version(void);
qs_context *qs_create(void);                     /* NULL on allocation failure */
void        qs_destroy(qs_context *);
const char *qs_error(const qs_context *);        /* "" after success */
void        qs_set_output(qs_context *, qs_output_fn, void *user);
void        qs_set_step_limit(qs_context *, size_t limit);

qs_program *qs_compile(qs_context *, const char *source, size_t length);
void        qs_program_destroy(qs_program *);
qs_status   qs_run(qs_context *, const qs_program *, int64_t *result);
qs_status   qs_eval(qs_context *, const char *source, size_t length, int64_t *result);
qs_status   qs_emit(qs_context *, const qs_program *, int format,
                    const unsigned char **data, size_t *length);
```

* `qs_output_fn` is `void (*)(int64_t value, void *user)` and receives every
  value printed by `print`/`say`, in order, after the run (also when the run
  traps after printing).
* `qs_compile` parses, checks, lowers, optimizes and schedules. Source is
  bytes with an explicit length; embedded NUL bytes are a syntax error.
* `qs_eval` = `qs_compile` + `qs_run` + `qs_program_destroy`.
* `qs_emit` formats: `QS_IL` (MetaIL text), `QS_BYTECODE` (MCIL binary,
  decodable with `metail::decode`), `QS_NATIVE` (x86-64 assembly text),
  `QS_OBJECT` (ELF64 relocatable), `QS_EXECUTABLE` (static ELF64 executable
  for x86-64 Linux). The buffer belongs to the context.

C++ (namespace `qscript`):

```cpp
struct Options {
    metachain::Options toolchain;     // IL limits, VM limits, optimize flag, JIT, ...
    std::size_t max_depth = 256, max_nodes = 100000;
    BackendOptions backend;           // scheduler, issue_width
    std::map<std::string, std::string, std::less<>> libraries;   // see chapter 4
    LibraryResolver resolver;         // std::function<std::optional<std::string>(std::string_view)>
    std::vector<std::string> library_paths;
    bool stdlib = true;
    std::size_t max_libraries = 256;
};
struct FunctionInfo { std::string name; std::size_t entry, arity; std::string file; std::size_t line; };
struct Unit { metail::Program program; std::vector<FunctionInfo> functions; std::vector<std::string> libraries; };

Result<AST> parse(std::string_view source, Options = {}, std::string file = {});
std::vector<Diagnostic> sema_check(const AST&, Options = {});
Result<Unit> lower(const AST&, Options = {});              // analysis + code generation
Result<Unit> finish(Unit, Options = {});                    // optimize + schedule
Result<Unit> compile_unit(std::string_view, Options = {}, std::string file = {});
Result<metail::Program> compile(std::string_view, Options = {}, std::string file = {});
Result<metail::Program> ast_to_ir(const AST&, Options = {});
Result<metail::Execution> run(const metail::Program&, Options = {});
Result<metail::Execution> call(const Unit&, std::string_view function,
                               const std::vector<std::int64_t>& args, Options = {});
const char* grammar_source();
std::vector<std::string> stdlib_names();
std::optional<std::string> stdlib_source(std::string_view name);
bool visit(const AST&, const std::function<bool(const dsl::ASTNode&)>&, std::size_t max_depth = 256);
// Backend stages, also usable on hand-written MetaIL:
Result<metail::Program> optimize(metail::Program, Options = {});
Result<metail::Program> schedule(metail::Program, Options = {});
Result<metail::Program> verify_ir(metail::Program, Options = {});
Result<std::string> emit(const metail::Program&, Options = {});
Result<std::string> select(const metail::Program&, Options = {});        // x86-64 assembly
Result<std::vector<std::uint8_t>> assemble(std::string_view, bool object = true, Options = {});
Result<std::vector<std::uint8_t>> link(const std::vector<std::vector<std::uint8_t>>&, Options = {});
Result<std::vector<std::uint8_t>> build(const metail::Program&, Options = {});
```

Use `qscript::run` rather than `metail::run` or `Toolchain::run` when you want
QScript diagnostics: it reports a failed `assert` as "assertion failed" with
its source line.

```cpp
qscript::Options o;
o.toolchain.vm.max_steps = 1'000'000;
auto unit = qscript::compile_unit(text, o, "script.qs");
if (!unit) { for (auto& d : unit.diagnostics) std::cerr << metachain::format(d) << "\n"; return 1; }
auto r = qscript::run(unit->program, o);
if (r) for (auto v : r->output) std::cout << v << "\n";
```

## 6.3 Runtime: `QScript-Runtime.h`

```c
typedef enum qs_engine { QS_ENGINE_INTERPRETER = 0, QS_ENGINE_JIT = 1 } qs_engine;
typedef enum qs_run_status { QS_RUN_HALTED = 0, QS_RUN_TRAPPED = 1, QS_RUN_STEP_LIMIT = 2 } qs_run_status;
typedef struct qs_limits {
    size_t max_steps, max_stack, max_instructions, max_depth, max_nodes, max_source_bytes;
} qs_limits;
typedef struct qs_run_info { int64_t result; size_t steps; size_t outputs; int status; } qs_run_info;

qs_status qs_get_limits(const qs_context *, qs_limits *);
qs_status qs_set_limits(qs_context *, const qs_limits *);   /* 0 fields keep the current value */
qs_status qs_set_engine(qs_context *, int engine);
int       qs_get_engine(const qs_context *);
qs_status qs_run_ex(qs_context *, const qs_program *, qs_run_info *);
qs_status qs_call(qs_context *, const qs_program *, const char *function,
                  const int64_t *args, size_t count, int64_t *result);
size_t      qs_program_size(const qs_program *);
size_t      qs_program_function_count(const qs_program *);
const char *qs_program_function_name(const qs_program *, size_t index);
size_t      qs_program_function_arity(const qs_program *, size_t index);
size_t      qs_program_library_count(const qs_program *);
const char *qs_program_library_name(const qs_program *, size_t index);
```

| Limit | Default | Bounds |
|-------|---------|--------|
| `max_steps` | 10,000,000 | executed instructions per run |
| `max_stack` | 1,000,000 | VM stack values; bounds recursion (a call uses 1 + arguments + saved registers) |
| `max_instructions` | 1,000,000 | size of a compiled program |
| `max_depth` | 256 | syntax / AST nesting; values above 256 are rejected |
| `max_nodes` | 100,000 | AST nodes per source |
| `max_source_bytes` | 64 MiB | bytes per source |

`qs_run_ex` returns `QS_OK` only for a normal halt, but fills `info` either
way: `steps` and `outputs` are meaningful for traps and step-limit stops too.
The JIT engine falls back to the interpreter on platforms without native code
support; results are identical.

`qs_call` runs one function with the given arguments and returns its value.
The program's top-level statements are not executed. Callable functions are
listed by `qs_program_function_*`: every function the program defines, plus
library functions it uses, named `lib::f`.

```c
for (size_t i = 0; i < qs_program_function_count(p); ++i)
    printf("%s/%zu\n", qs_program_function_name(p, i), qs_program_function_arity(p, i));
```

## 6.4 Libraries: `QScript-Library.h`

```c
size_t      qs_stdlib_count(void);
const char *qs_stdlib_name(size_t index);                        /* sorted; NULL past the end */
const char *qs_stdlib_source(const char *name, size_t *length);  /* NULL if unknown */
qs_status   qs_add_library(qs_context *, const char *name, const char *source, size_t length);
qs_status   qs_add_library_path(qs_context *, const char *directory);
qs_status   qs_set_stdlib(qs_context *, int enable);
typedef const char *(*qs_library_resolver)(const char *name, size_t *length, void *user);
qs_status   qs_set_library_resolver(qs_context *, qs_library_resolver, void *user);
qs_status   qs_clear_libraries(qs_context *);
```

The lookup order is in [chapter 4](04-libraries.md). A resolver returns the
library text and sets `*length`, or returns NULL for "not mine"; the text
must stay valid until the compile call that asked for it returns.

```c
static const char *from_table(const char *name, size_t *length, void *user) {
    const struct script *s = find_script((struct table *)user, name);
    if (!s) return NULL;
    *length = s->length;
    return s->text;
}
qs_set_library_resolver(context, from_table, &my_table);
```

## 6.5 Modules: `QScript-Module.h`

```c
qs_module *qs_module_create(void);
void       qs_module_destroy(qs_module *);
qs_status  qs_module_add_source(qs_module *, const char *text, size_t length, const char *file);
qs_status  qs_module_add_file(qs_module *, const char *path);
qs_status  qs_module_add_library(qs_module *, const char *name, const char *text, size_t length);
size_t     qs_module_sources(const qs_module *);
size_t     qs_module_library_count(const qs_module *);
void       qs_module_clear(qs_module *);
qs_status  qs_module_compile(qs_context *, const qs_module *, qs_program **program);
```

C++: `qscript::module::Module {sources, libraries}`, `parse_module`,
`compile_module` (returns a `metail::Program`) and `compile_module_unit`
(returns a `Unit`). Module libraries take precedence over the context's.

## 6.6 Diagnostics: `QScript-Diagnostics.h`

```c
typedef enum qs_stage {
    QS_STAGE_PARSE, QS_STAGE_VERIFY, QS_STAGE_OPTIMIZE, QS_STAGE_EXECUTE,
    QS_STAGE_ASSEMBLE, QS_STAGE_LINK, QS_STAGE_LOWER, QS_STAGE_REWRITE, QS_STAGE_IO
} qs_stage;
typedef struct qs_diagnostic {
    int stage; const char *message; const char *file; size_t line, column, offset;
} qs_diagnostic;

size_t      qs_diagnostic_count(const qs_context *);
qs_status   qs_diagnostic_get(const qs_context *, size_t index, qs_diagnostic *);
const char *qs_stage_name(int stage);
qs_status   qs_check(qs_context *, const char *source, size_t length);
qs_status   qs_ast_dump(qs_context *, const char *source, size_t length, const char **text, size_t *text_length);
qs_status   qs_match_tree_dump(qs_context *, const char *source, size_t length, const char **text, size_t *text_length);
```

After any failing operation, the diagnostics behind `qs_error()` are
available one by one. Stages: `parse` for syntax errors, `verify` for
semantic errors (undefined names, arity, scope), `lower` for code-generation
limits, `execute` for traps (with `offset` = instruction index and the source
position of the failing statement). `line`/`column` are 0 when unknown.

```c
if (qs_check(c, src, len) != QS_OK)
    for (size_t i = 0; i < qs_diagnostic_count(c); ++i) {
        qs_diagnostic d; qs_diagnostic_get(c, i, &d);
        printf("%s:%zu:%zu: %s error: %s\n", d.file, d.line, d.column, qs_stage_name(d.stage), d.message);
    }
```

## 6.7 Backend: `QScript-Backend.h`

```c
typedef enum qs_scheduler { QS_SCHED_LIST = 0, QS_SCHED_CYCLE = 1, QS_SCHED_STATIC = 2 } qs_scheduler;
typedef struct qs_backend_options { int scheduler; unsigned issue_width; int optimize; } qs_backend_options;

qs_status   qs_set_backend(qs_context *, const qs_backend_options *);   /* issue_width 0 / optimize <0 keep */
qs_status   qs_get_backend(const qs_context *, qs_backend_options *);
qs_status   qs_set_optimize(qs_context *, int enable);
qs_status   qs_set_scheduler_name(qs_context *, const char *name);     /* "list", "cycle", "static" */
const char *qs_scheduler_name(int scheduler);
qs_status   qs_backend_describe(const qs_context *, char *text, size_t capacity, size_t *length);
qs_status   qs_backend_report(qs_context *, const qs_program *, const char **text, size_t *length);
```

Scheduling reorders instructions only inside basic blocks (calls, returns,
pushes, pops and prints are barriers), so every configuration gives the same
results. `qs_backend_report` lists the basic blocks of a compiled program
with their critical path and scheduled makespan. C++ helpers:
`qscript::backend::describe`, `scheduler_from_name`, `report`,
`schedule_block`.

## 6.8 Grammars: `QScript-Grammar.h`

```c
qs_grammar *qs_grammar_compile(qs_context *, const char *source, size_t length);
qs_grammar *qs_grammar_qscript(qs_context *);          /* the QScript grammar, compiled */
const char *qs_grammar_qscript_source(void);
void        qs_grammar_destroy(qs_grammar *);
const char *qs_grammar_name(const qs_grammar *);
size_t      qs_grammar_rule_count(const qs_grammar *);
const char *qs_grammar_rule_name(const qs_grammar *, size_t index);
qs_status   qs_grammar_peg(qs_context *, const qs_grammar *, const char **text, size_t *length);

qs_match       *qs_grammar_parse(qs_context *, const qs_grammar *, const char *input, size_t length,
                                 const char *start /* NULL = "TOP" */);
void            qs_match_destroy(qs_match *);           /* roots only */
const char     *qs_match_name(const qs_match *);
const char     *qs_match_sym(const qs_match *);
const char     *qs_match_text(const qs_match *, size_t *length);
qs_status       qs_match_span(const qs_match *, qs_span *);   /* {from, to, line, column} */
size_t          qs_match_count(const qs_match *);
const qs_match *qs_match_child(const qs_match *, size_t index);
const qs_match *qs_match_find(const qs_match *, const char *name);
qs_status       qs_match_walk(const qs_match *, qs_match_visitor, void *user);
qs_status       qs_match_dump(qs_context *, const qs_match *, const char **text, size_t *length);
```

Child matches belong to their root; only roots from `qs_grammar_parse` are
destroyed. A visitor (`int (*)(const qs_match *, size_t depth, void *user)`)
returns non-zero to descend into children. The C++ side
(`qscript::grammar::Grammar`, `Match`, `ParseOptions`, `ParseOutcome`) is
described in [chapter 5](05-grammars.md); `qs_grammar` and `qs_match` are the
same objects as `Grammar` and `Match`.

```c
static int show(const qs_match *m, size_t depth, void *user) {
    size_t n; const char *t = qs_match_text(m, &n);
    printf("%*s%s = %.*s\n", (int)(2 * depth), "", qs_match_name(m), (int)n, t);
    return 1;
}
qs_grammar *g = qs_grammar_compile(c, src, strlen(src));
qs_match *m = g ? qs_grammar_parse(c, g, input, strlen(input), NULL) : NULL;
if (m) qs_match_walk(m, show, NULL); else fprintf(stderr, "%s\n", qs_error(c));
qs_match_destroy(m);
qs_grammar_destroy(g);
```

## 6.9 ABI notes

* Enumerations are passed as `int` so unknown values can be rejected
  (`QS_INVALID_ARGUMENT`) instead of causing undefined behaviour.
* Structures (`qs_limits`, `qs_run_info`, `qs_diagnostic`, `qs_span`,
  `qs_backend_options`) contain only `int`, `size_t`, `int64_t` and pointers.
* All entry points catch C++ exceptions; none propagates into C.
* New in this version (all additive; existing entry points are unchanged):
  `QScript-Runtime.h`, `QScript-Library.h`, `QScript-Diagnostics.h`,
  `QScript-Grammar.h`; `qs_get_backend`, `qs_set_scheduler_name`,
  `qs_scheduler_name`, `qs_backend_report`; `qs_module_add_file`,
  `qs_module_add_library`, `qs_module_library_count`, `qs_module_clear`.
