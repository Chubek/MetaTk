# MetaLD embedding and extension API

`MetaLD.h` is an ISO C11 embedding ABI; `MetaLD.hpp` provides move-only C++20
owners. Link `MetaLD::metald`. Enable `ARCHDB_BUILD_METALD=ON` (default on POSIX).
MetaLD itself needs only C and the full MetaArch runtime; it can be built with
the C++ assembler/DSL disabled. The current executable targets are **ELF64 x86-64
and ELF32 i386, little-endian**, matching ArchLD.

## Same-process assembly and linking

```cpp
#include <MetaAS/MetaAS.hpp>
#include <MetaLD/MetaLD.hpp>

auto options = metaas::options();
options.target = "amd64";
metaas::Assembler assembler(options);
auto object = assembler.compile(
    ".text\n.globl _start\n_start:\n"
    "mov $exit_code,%edi\nmov $60,%eax\nsyscall\n");

metald::Linker linker;
linker.add("main.o", object.bytes()).define("exit_code", 42).link();
linker.write_executable("answer", "_start"); // Linux executable, exit status 42.
```

The C equivalents are `metald_options_init`, `metald_create`,
`metald_add_memory`, `metald_define`, `metald_link`,
`metald_write_executable`, and `metald_free`. `metald_add_file` accepts ordinary
ELF objects and `ar` archives; `metald_add_memory` copies them from host buffers.
`metald_image`/`metald_image_size` expose relocated image bytes, and symbol
enumeration/lookup exposes final addresses. These addresses correspond to the
configured image base, **not** the address of the returned host buffer.

Default options select `ELF64` with image base `0x401000`. The catalogue is selected
from the explicit option, `METALD_CATALOGUE`, installed data or source data.
Explicit catalogues can use all full-runtime formats. Always initialize options;
ABI version and struct size are validated. Strings need survive only `create`.

## Extension points

- **Input providers:** feed object/archive bytes with `metald_add_memory`; the host
  can load from an archive service, network cache, compiler, or MetaAS result.
- **Absolute definitions:** `metald_define` / `Linker::define` adds a strong
  absolute symbol before linking. Values appear in image relocations, symbol
  queries, maps, and executable symbol tables (`SHN_ABS`). Strong duplicates fail;
  ELF32 definitions must fit 32-bit addresses. Definitions can suppress archive
  extraction for a symbol they already satisfy. ELF64 reserves
  `_GLOBAL_OFFSET_TABLE_` for the linker's own GOT and rejects host overrides.
- **Lazy resolver:** `metald_set_resolver` installs a callback for strong symbols
  still missing after archive extraction. Return 1 with an address, 0 if missing,
  or -1 with an optional bounded error message. Successful resolutions are cached
  once per symbol. Object/archive definitions take precedence; unresolved weak
  symbols keep the existing zero fallback. NULL disables the resolver.

```cpp
linker.resolver([](std::string_view name) -> std::optional<uint64_t> {
    if (name == "host_table") return 0x600000;
    return {};
});
```

Resolver exceptions in C++ are translated into positioned symbol/input errors.
Callback `user` remains owned by the C caller through context destruction; the
C++ wrapper owns its callable. Callbacks may not reenter/free the active linker.

## Lifecycle and diagnostics

The state machine is `COLLECTING -> LINKED` or `FAILED`. Add/define/resolver
configuration is permitted only while collecting. Linking is one shot.
Operational parse/link/write failures are sticky: inspect `metald_error`, destroy
and recreate the context. Invalid argument/state requests leave the state intact.
Read-only symbol queries return `METALD_NOT_FOUND` for absent names/indices and
never mutate the context. `metald_error` is a borrowed bounded NUL-terminated
diagnostic until the next mutation or free. No API calls `exit`.

After successful linking, concurrent image/symbol queries are supported. Mutation
and destruction require exclusive access; contexts otherwise own independent
state. C++ `image()` returns a borrowed span; `symbols()` returns copied records.
The library allocates no executable memory and does not invoke linked code.
`write_executable` uses the existing atomic ELF writer with W-X load segments.

## Runtime separation and building

The top-level build compiles **only** the CWEB-generated `ArchLD/src/ArchLD.c`
against the full MetaArch/ArchBFD implementation. MetaAS and MetaLD can therefore
be linked into one application with exactly one Dsymspec runtime. The separate
`ArchLD/CMakeLists.txt` build retains its original JSON satellite and must not be
linked into that application. New core logic belongs in `ArchLD/src/ArchLD.w`;
regenerate using `ctangle src/ArchLD.w - src/ArchLD.c` in `ArchLD`.

Headers install under `include/metaarch/MetaLD`; static consumers link `metald`,
`metaarch` and its configured codec dependencies. CMake propagates them. The
underlying POSIX ELF writer currently requires a little-endian host and compiler
support for private checked 128-bit arithmetic. Public headers remain ISO C11.

```sh
cmake --build build-archas --parallel 4
ctest --test-dir build-archas --output-on-failure -R 'embedding|metald'
```

Tests assemble real ELF32/ELF64 inputs, link MetaAS and MetaLD in the same C/C++
process, check host definitions/resolver precedence and callback failures, inspect
outputs with `readelf`, and execute amd64 outputs. The complete standalone
ArchLD object/archive/hardening suites also run against the full-runtime build.
