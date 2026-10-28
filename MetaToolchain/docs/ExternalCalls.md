# External C ABI calls and linker metadata

Implemented on 2026-10-28 across MetaArchIR (the assembly IR in this checkout),
MetaIL, and Juniper. No architecture specification, generated backend, optimizer
kernel, or required runtime dependency was changed.

## API additions and compatibility

- **MetaArchIR:** `ExternalFunction`, `Program::extern_function`,
  `Program::call_external`, `external_functions`, `link_libraries`,
  `link_arguments`, `linkage_json`, and `Image::imports`. New `extern` and
  `call_extern` statements; `marchir --emit linkage`. Encoded references to import
  aliases resolve to exported symbols. JSON images add a `linkage` member; flat
  output rejects loss of import metadata. ELF native assembly embeds LLVM's
  `SHT_LLVM_DEPENDENT_LIBRARIES` section (`0x6fff4c04`).
- **MetaIL:** canonical `externals` array, external declaration syntax, ABI type
  and call-arity/void validation; `metail_external`, trailing
  `metail_module.externals/external_count`, `metail_linkage`,
  `metail_module_linkage`; C++ `ExternalFunction`, `Module::extern_function`,
  `Module::externals`, and both `Context::linkage` overloads. `metail linkage`
  extracts the manifest. **C consumers must recompile** for the extended module
  descriptor layout and zero-initialize unused fields. Owned module snapshots
  deep-copy all new strings and arrays.
- **Juniper:** owned external declarations on `Module` and `MachineModule`,
  `Compiler::linkage`, `emit_linkage_json`, and `Instruction::external` import
  indices. Selection resolves exported symbols; scheduling validates and preserves
  declarations, indices and effect fences. JSON output includes linkage and call
  indices. New `@extern` / `@extern_symbol` macros, `linkage` CLI command and CMake
  output mode. The optional MetaArch adapter adds `ExternalCallBinding` and
  `encode_external_call`, retaining symbol relocations and library imports.

Existing undeclared calls remain legal in MetaIL/Juniper and generate no inferred
library dependency. Existing encoders, native instructions, relocation semantics
and optimizer purity rules are unchanged. No call becomes pure merely because it
is imported from libc. Unused declarations are retained.

## Manifest and linker contract

Schema 1 has declaration-ordered `imports` and first-declaration-ordered,
deduplicated `libraries` / GNU-compatible driver `arguments`. Each import carries
its local name, exported symbol, library and `calling_convention: "c"`.
MetaIL/Juniper additionally retain ABI parameter/result types and a variadic flag.
Library `"c"` becomes `-lc`, `"m"` becomes `-lm`; a path containing `/` is passed
as one argument (use `./libfoo.so` for a local file). Relative paths use the link
working directory. Append arguments **after objects** through an argv API, not a
shell. Controls, option prefixes and response-file prefixes are rejected.

LLD consumes embedded ELF `.deplibs` automatically. GNU ld and other drivers use
the manifest; no claim is made that they understand the LLVM extension. The ELF
section contract follows LLVM's `lld/ELF/InputFiles.cpp`,
`addDependentLibrary` / `SHT_LLVM_DEPENDENT_LIBRARIES` handling:
<https://github.com/llvm/llvm-project/blob/main/lld/ELF/InputFiles.cpp>.
Sysroot, library search paths, runtime loader paths and library availability
remain explicit driver responsibilities. Compilation never dlopens a user library.

## Boundaries

MetaArchIR accepts exported assembler identifiers and caller-written native ABI
setup, so calls are not limited to a built-in libc function list. A C++ export
needs its actual mangled symbol or a C wrapper. It does not allocate registers,
marshal arguments or implement an object writer. `call_external` emits native
assembly; encoded images use explicit encoder/parameter/addend bindings.

MetaIL/Juniper remain scalar signed-64 IR, not new native compilers. Supported ABI
metadata types are `i8/u8/i16/u16/i32/u32/i64/u64/ptr`, plus void results and scalar
variadics. Floating-point, aggregate and non-C signatures need explicit scalar C
wrappers at these layers. The target lowerer must implement integer extension,
pointer-width checks, argument/result moves, stack/save conventions and variadic
setup. The adapter encodes only the selected branch and does not bypass this work.
No automatic host symbol addresses or guessed non-x86 call encoders are used.

## Verification

Environment: Linux x86-64, GCC 16.2.1, Clang/LLD 22.1.8, real Lua 5.4. Builds use
`-Wall -Wextra -Wpedantic -Werror`; sanitizer trees add
`-fsanitize=address,undefined -fno-omit-frame-pointer`. There were no compiler
warnings. yyjson emits its existing CMake minimum-version deprecation warning.
Ninja is unavailable, so new Unix Makefiles build trees were used; existing
relocated/stale build directories and the user's Juniper manual work were left
untouched.

| Suite | Before (GCC Release) | After |
|---|---|---|
| MetaArch / MetaArchIR | 25/25, 12.49 s | 25/25 on GCC and Clang Release; 25/25 on both ASan+UBSan builds |
| Juniper + embedded MetaIL | 18/18, 4.54 s, adapter off | 21/21 on GCC and Clang Release and ASan+UBSan, adapter on |
| Standalone MetaIL, including examples | not separately measured | 19/19 GCC Release |

Observed after-run wall times: MetaArch Release GCC 14.23 s / Clang 14.14 s;
ASan+UBSan GCC 95.44 s / Clang 94.37 s. Juniper's expanded 21-test matrix took
6.12 s / 6.61 s in Release and 14.80 s / 13.69 s under sanitizers. These ran
concurrently with other verification jobs and are not performance measurements.
The final changed MetaArchIR tests were also rerun on Clang ASan+UBSan (13/13).

`metail verify` passed **230 rewrites across 183473 signed-64 cases**, including
2470 effect/trap cases. The generated-kernel check passed with all 327 kernels
unchanged. Tests cover import ownership, parser-free structured paths,
parse/emit/optimization retention, void/variadic/arity failures, malformed metadata,
JSON manifest agreement, alias relocations, flat-output rejection, scheduling
indices/fences and macro/CLI integration.

The Linux AMD64 native test compiles an independent custom `.so` in a path with
spaces, carries `labs` and the custom import through Juniper → MetaIL optimization
→ selection/scheduling → MetaArchIR, checks branch relocation metadata, links using
the generated manifest, and executes `custom_twice(labs(-21)) == 42`. It also links
and executes with LLD using **no explicit library arguments**. ABI setup in this
fixture is deliberately explicit, not a hidden production ABI lowerer. MetaArchIR
has its own independent native libc/shared-library regression. AArch64 `bl puts`
and its dependency section were separately assembled by Clang's cross assembler.

### Reproduce

From `MetaToolchain/` (use a fresh build directory):

```sh
cmake -S . -B /tmp/opencode/foreign-arch -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DARCHDB_BUILD_ARCHAS=OFF -DARCHDB_BUILD_METAAS=OFF -DARCHDB_BUILD_METALD=OFF
cmake --build /tmp/opencode/foreign-arch -j4
ctest --test-dir /tmp/opencode/foreign-arch --output-on-failure -j4

cmake -S ../Juniper -B /tmp/opencode/foreign-juniper -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release -DJUNIPER_WITH_METAARCH=ON \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build /tmp/opencode/foreign-juniper -j4
ctest --test-dir /tmp/opencode/foreign-juniper --output-on-failure -j4
/tmp/opencode/foreign-juniper/metail/metail verify
python3 MetaIL/tools/generate_kernels.py --check
```

Repeat in new trees with `-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`.
For sanitizer runs choose Debug and append the sanitizer flags above to both
language flags. Standalone MetaIL uses the same configure/build/test commands
with `-S MetaIL`. `Juniper/examples/external.jun`, `MetaIL/examples/external.mi`
and `MetaArchIR/examples/cli/amd64/libc.mair` demonstrate the new source interfaces.
