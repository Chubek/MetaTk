# Command-line tools

## `archdb` (C core frontend)

```text
archdb validate [--strict] [--lint] [--backend] [-i FMT] FILE...
archdb convert -f FMT [-o OUT] [-i FMT] [--embed-backend|--ref-backend]
               [--no-backend-load] [--compact] [--no-header] FILE
archdb list [CATALOGUE|DIR]
archdb catalogue DIR [-o OUT] [-f FMT] [--root ROOT]
archdb encode [-i FMT] FILE ENCODER [name=value ...] [--target N]
archdb dump-ir [-i FMT] FILE [ENCODER]
archdb info [-i FMT] FILE
```

`FMT` is `yaml`, `json`, `xml` or `sexpr`. Input normally follows the filename
extension; `-i` overrides it. `-o -` or omitted output writes to stdout. Put
validation flags before the files they should affect. Exit codes currently are
0 for success, 1 for invalid input/operation/I/O failure, and 2 for usage errors;
there is no distinct I/O status 3 yet. Diagnostics are text on stderr.

```sh
build/archdb validate --lint ArchDB/*.dsymspec.yaml
build/archdb convert -f json --ref-backend -o riscv64.json ArchDB/riscv64.dsymspec.yaml
build/archdb convert -f xml --embed-backend -o riscv64.xml ArchDB/riscv64.dsymspec.yaml
build/archdb list ArchDBCatalogue.yaml
build/archdb catalogue ArchDB --root ArchDB -o /tmp/catalogue.yaml
build/archdb dump-ir ArchDB/riscv64.dsymspec.yaml addi
build/archdb encode ArchDB/riscv64.dsymspec.yaml addi rd=10 rs1=10 imm=-1
```

`validate --lint` includes advisory schema/model/program checks. Strict mode
rejects warnings, but not all historical lint notes have been promoted to
warnings. See [the diagnostic table](encoding.md). `--backend` also loads and
parses the referenced backend.

`encode` selects an **encoder name**, not an instruction mnemonic or form. Its
arguments are numeric hardware register encodings/immediates; register-name
strings are unsupported here. It uses the low-level path and does not enforce
form operand metadata. For register names or assembly text use `metaarch`.

`catalogue` generates summaries from a directory scan. Review its output before
replacing the committed catalogue. There is no `--check` implementation. Lookup
supports catalogue name, architecture and subarchitecture, not general triples
or a configurable case-insensitive alias database.

## `archbfd` (object-format frontend)

```text
archbfd validate [--strict] [--lint] [-i FMT] FILE...
archbfd convert -f FMT [-o OUT] [-i FMT] [--compact] FILE
archbfd list [CATALOGUE|DIR]
archbfd catalogue DIR [-o OUT] [-f FMT] [--root ROOT]
archbfd info [-i FMT] FILE
archbfd relocs [-i FMT] FILE [NAME]
archbfd records [-i FMT] FILE [NAME]
archbfd object [--catalogue FILE] [-f FORMAT] OBJECT
```

`archbfd` works on the `ArchBFD` catalogue: the object formats that BFD
implements. `validate` checks a document against the MetaBFD schema rules,
`convert` rewrites it in any of the four formats, `info`/`relocs`/`records`
print the description, and `object` reads an object file with the description
that matches it.

```sh
build/archbfd validate ArchBFD/*.bfd.yaml
build/archbfd list ArchBFDCatalogue.yaml
build/archbfd relocs ArchBFD/ELF64.bfd.yaml R_X86_64_PC32
build/archbfd records ArchBFD/ELF64.bfd.yaml ehdr
build/archbfd object build/archdb
```

`object` picks the description whose magic matches the file and that can read
it; `-f` forces one. The reader understands ELF, COFF and PE objects today.
Exit codes are the same as `archdb`: 0 success, 1 invalid input, 2 usage.

See [the object-format guide](bfd.md) for the document format and the API.

## `metaarch` (optional C++20 frontend)

```text
metaarch assemble SPEC SOURCE [-o BINARY] [--target N] [--bind ENCODER=PARAM,...]
metaarch convert SPEC -f yaml|json|xml|sexpr [-o OUTPUT]
```

`SPEC` accepts `.march` or any core format. `SOURCE=-` reads stdin. Assembly
without `-o` prints hex; with `-o` writes raw bytes. Relocations are refused by
the CLI because a raw binary cannot carry their records.

```sh
build/metaarch assemble examples/toy.march examples/toy.asm
# 11 ff 10 2a
build/metaarch convert examples/toy.march -f yaml -o toy.yaml
printf 'addi a0, a0, -1\n' | build/metaarch assemble \
  ArchDB/riscv64.dsymspec.yaml - --bind addi=rd,rs1,imm
```

See [DSL and assembler semantics](dsl.md) for explicit positional bindings and
unsupported assembly constructs. Commands named `lint`, `diff` or `fmt`, SARIF/
JSON diagnostic formats, `--allow=CODE`, and form/mnemonic-selecting `archdb
encode` are planned work, **not available interfaces**.
