# MetaAS embedding and extension API

`MetaAS.h` provides an ISO C11 ABI over ArchAS. `MetaAS.hpp` adds move-only C++20
owners. Link `MetaAS::metaas` when using the MetaToolchain CMake project. The API
builds with `ARCHDB_BUILD_METAAS=ON` and requires `ARCHDB_BUILD_ARCHAS=ON`.

## Assemble in memory

```c
#include <MetaAS/MetaAS.h>
#include <stdio.h>
#include <string.h>

MetaASOptions options;
metaas_options_init(&options);
options.target = "amd64";
options.backend = METAAS_BUILTIN;
options.format = METAAS_HEX;
MetaASError error;
MetaAS *as = metaas_create(&options, &error);
if (!as) { fprintf(stderr, "%s\n", error.message); return 1; }
const char *source = "encode ret\n";
MetaASResult *result = metaas_compile(as, source, strlen(source), "unit.mair");
if (!result) {
    fprintf(stderr, "%s\n", metaas_error(as)->message);
    metaas_free(as);
    return 1;
}
fwrite(metaas_result_data(result), 1, metaas_result_size(result), stdout); /* c3 */
metaas_free(as);
metaas_result_free(result);
```

Set `format = METAAS_OBJECT` and `backend = METAAS_AUTO` for a real object from
GAS/LLVM. `METAAS_ASSEMBLY`, `METAAS_BINARY`, `METAAS_HEX`, and `METAAS_JSON` have
the same contracts as [domas](../ArchAS/README.md). Object bytes can be passed
directly to `metald_add_memory` in the same process. The builtin backend needs no
native assembler and uses the MetaArch/Dsymspec encoders/forms directly.

Options cover the target/specification/database, target index, syntax, output,
backend, native tool/triple, and resource limits. Always call `metaas_options_init`;
the version and struct size are checked before use. `metaas_add_include`,
`metaas_add_lua`, `metaas_add_plugin`, `metaas_add_assembler_argument` and
`metaas_define` extend configuration. All strings are copied.

## Custom directives

`metaas_register_directive(as, "name", callback, user)` registers an in-process
directive with the same `ArchASInvocation`/`ArchASEmitter` protocol as an ArchAS
shared-library plugin. The invocation provides the source location, target,
syntax and length-delimited argument text. Emit source with `emitter->emit`,
report errors with `emitter->error`, and return zero on success. Emitter failures
are propagated even when the callback ignores the return value. Names are copied;
the caller owns `user` through the assembler's lifetime. Reserved/duplicate names
are rejected. Custom directives participate in normal macro/include expansion.

Lua and shared-library extensions work through `metaas_add_lua` and
`metaas_add_plugin`; optional build switches retain their ArchAS behavior.

```cpp
#include <MetaAS/MetaAS.hpp>
auto options = metaas::options();
options.target = "amd64";
options.backend = METAAS_BUILTIN;
options.format = METAAS_HEX;
metaas::Assembler as(options);
as.directive("pair", [](std::string_view arg, const ArchASInvocation &) {
    return ".byte " + std::string(arg) + "," + std::string(arg);
});
auto result = as.compile(".pair 7\n", "pair.mair"); // result.text() == "0707\n"
```

## Ownership, errors, concurrency

- Every compile creates a fresh macro table, Lua VM and plugin instance set.
  Registered host directives and configuration remain until the assembler is freed.
- Compile borrows input only during the call. The result owns bytes, diagnostic
  text and dependency paths, and survives assembler destruction. Returned views
  borrow from the result until `metaas_result_free`.
- C functions contain C++ exceptions and return status/NULL. `MetaASError` holds
  a status, file/line/column and bounded NUL-terminated message, including OOM.
  Each operation replaces the last error. A failed compilation permits retry.
- C++ methods throw `metaas::Error` (with copied diagnostic detail) or allocation
  exceptions. Callback exceptions are caught at the trampoline and reported as
  assembly errors. Results and assemblers are move-only; callback storage moves
  with its assembler.
- Serialize operations on each assembler. Distinct assemblers may run concurrently
  if their user callbacks are thread-safe. Results support concurrent const reads.
  Callbacks must neither reenter nor free their active assembler.

Headers install under `include/metaarch/MetaAS`, libraries under the configured
library directory. For direct static linking, use the C++ linker with `metaas`,
`archas`, `metaarch_dsl`, `metaarch`, their configured codec/Lua libraries and the
platform dynamic-loader library. CMake propagates these dependencies automatically.

## Native C++20 DSL

Include `<MetaAS/MetaAS-DSL.hpp>` and link `MetaAS::dsl`. This header builds on
`DSLtk.hpp` (selected by `ARCHDB_DSLTK_DIR`); DSLtk supplies pipeline stages, the
CRTP feature mixin and compile-time string parameters. It adds no runtime library.
The existing C ABI and `MetaAS.hpp` remain usable without including DSLtk.

```cpp
#include <MetaAS/MetaAS-DSL.hpp>

using namespace metaas::dsl;
auto options = metaas::options();
options.target = "amd64";
options.backend = METAAS_BUILTIN;
options.format = METAAS_HEX;
metaas::Assembler assembler(options);

auto program = Program{}
    | section(".text") | global("answer") | label("answer")
    | encode("movimm", {arg<"dst">(reg("rax")), arg<"imm">(imm(42))})
    | encode("ret");
auto output = program | compile(assembler, "answer.cpp");
// output.text() == "48b82a00000000000000c3\n"
```

The equivalent fluent API is `program.section(...).global(...).label(...)`
followed by `program.encode(...)` and `program.compile(assembler)`. Encoder and
register names come from the chosen architecture database. The same program can
be compiled against multiple targets when they expose compatible encoders.
`arg("name", value)` is the runtime-name equivalent of `arg<"name">(value)`.

| API | Contract |
| --- | --- |
| `reg(name)` | Owns a validated physical-register identifier; resolved by the compiler. |
| `imm(value)` | Accepts integral values fitting signed 64 bits; rejects bool and floating point. |
| `symbol(name, addend)` | An owned relocation reference with a signed addend. |
| `encode(name, bindings)` | Uses low-level encoder constraints; parameter names are explicit. |
| `form(index, bindings)` | Also checks declared form operand constraints; binding order follows form operands. |
| `section`, `global`, `label` | Validated identifiers; section names start with a dot; duplicate labels fail. |
| `align(bytes)` | Power of two, from 1 through 65536, in bytes. |
| `data(Width, {values...})` | Byte/word/long_/quad target-endian integers, with width checks. |
| `bytes(span)`, `ascii(string_view)` | Literal byte data; empty inputs do nothing. No terminator is added. |
| `source()`, `size()`, `empty()`, `clear()` | Inspect generated source/statement count or reset the program. |

For example, RISC-V form 11 accepts
`form(11, {arg("rd", reg("x10")), arg("rs1", reg("x10")), arg("imm", imm(-1))})`.
Form indices belong to the selected specification, not to a stable instruction
numbering API. Symbol bindings currently require `encode`, because the underlying
form API does not accept symbolic inputs. Use JSON or object output to retain
relocations; flat byte/hex output rejects unresolved relocations.

`data` accepts signed minima through unsigned maxima for 8/16/32-bit fields;
quad values are signed `int64_t`. Use `bytes` for other 64-bit bit patterns.
`ascii` emits numeric byte directives, so quotes, newlines, NUL, invalid UTF-8,
comment markers and Lua template delimiters remain literal data. Integer rendering
is locale-independent. Arbitrary native assembly, macros, Lua and custom directive
invocations remain available through `assembler.compile(source, file)`.

Programs own all their input text. Copies are independent; moving leaves the
source program empty and reusable. Fluent operations validate before appending;
failed operations leave the program unchanged. Pipeline stages own their names
and bindings and can be reused. Passing an lvalue program to a stage copies it;
passing an rvalue moves it. The terminal `compile(assembler)` stage **borrows** the
assembler, which must outlive its invocations. Output retains MetaAS result
ownership and survives assembler destruction.

`Program({max_source_bytes, max_statements})` bounds generated source (default
16 MiB / 1,000,000 statements). These builder limits are separate from the
assembler's source, output and expansion limits. One generated line is one
statement; diagnostics use the supplied filename and generated line/column.
Identifiers and bindings are validated during construction, while target-specific
registers, form ranges and encoders are checked during compilation.

Builder misuse throws `std::invalid_argument`, range failures throw
`std::out_of_range`, and resource limits throw `std::length_error`. Compilation
throws `metaas::Error` with the existing diagnostic record. Allocation can throw
`std::bad_alloc`. Const program reads permit concurrency; mutations and operations
on an assembler require caller synchronization.

The ownership wrapper also provides `Assembler::assembler_argument`, rejects
embedded NULs in its string setters and diagnostic filename, and destroys the old
assembler before releasing callback captures on move assignment. C option strings
remain ordinary NUL-terminated strings. A moved-from assembler rejects operations
with `std::logic_error`.

Build and run the standalone example and tests from the MetaToolchain root:

```sh
cmake -S . -B build -DARCHDB_BUILD_METAAS=ON \
  -DARCHDB_DSLTK_DIR=/mnt/nvme/metatk/DSLtk
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure -R 'metaas_dsl|embedding|archas'
```

CMake installs the new header and DSLtk beside the existing MetaAS include tree.
`MetaAS::dsl` is an in-tree CMake target, like the existing `MetaAS::metaas`;
installed consumers currently use the documented direct-link dependencies.
