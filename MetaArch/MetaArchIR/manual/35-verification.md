# Chapter 35 — Verification, troubleshooting and maintenance

## Run the existing checks

```sh
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure -R 'metaarch_ir|mair_example'
```

The MetaArchIR unit tests exercise parsing, form constraints, byte strings,
sections, relocations, limits and database loading. Every API example is built
and run. CLI tests check output modes, status codes, source locations and output
preservation. LLVM tools, when available, independently check selected AArch64
and RISC-V encodings. Native Compute/GCD execution is limited to the supported
x86-64 Linux host configuration.

For a complete project check, omit `-R`. Use separate GCC and Clang build trees
and enable `-Wall -Wextra -Wpedantic -Werror` when validating changes. A Debug
ASan/UBSan build should add `-fsanitize=address,undefined
-fno-omit-frame-pointer` to both C and C++ flags. See the root
[`REPORT.md`](../../REPORT.md) for recorded verification observations; historical
timings are not a guarantee for your machine.

## Diagnose the failing layer

| Symptom | Investigate |
| --- | --- |
| Database not found | Root layout, `--db`, `METAARCHDB`, working directory |
| Unknown register | Database name versus native `%` decoration |
| Parameter not bound | Exact encoder parameter names and complete binding set |
| Form operand error | Form index, declared order, class, width and immediate bounds |
| Required feature error | Selected target record and form requirements |
| Native text cannot be encoded | Use text rendering or explicit encoder/form statements |
| Unresolved relocation | Preserve the Image/JSON and hand off to a linker |
| Flat output requires one section | Choose an image output instead of guessing layout |
| Byte budget exceeded | Source size, padding, escaping and serialization expansion |

## Maintain trustworthy examples

When changing a database revision, recheck hard-coded form indices and expected
bytes. For an encoding correction, compare against an independent assembler or
authoritative ISA reference; a round trip through the same implementation is not
independent evidence.

Keep regression cases that distinguish native mnemonic recognition, low-level
encoder checks and checked-form validation. Do not make an invalid form pass by
silently routing it through a less constrained encoder interface. Add focused
examples for new supported behavior, document unsupported behavior precisely,
and keep assembly decisions explicit. That discipline preserves MetaArchIR's
role as a maintainable portable-assembly layer.

[Previous](34-retargeting.md) · [Contents](README.md)
