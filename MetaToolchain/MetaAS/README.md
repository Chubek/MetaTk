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
