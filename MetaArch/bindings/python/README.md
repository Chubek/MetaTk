# MetaArch Python bindings (nanobind)

Python 3.9+ bindings for the opaque MetaArch C ABI v1, implemented in C++17 with
[nanobind](https://github.com/wjakob/nanobind). There is no ctypes layer, runtime
library discovery, or `METAARCH_LIBRARY` variable. The native extension links to
the CMake `metaarch` target; static and shared MetaArch builds are supported.
Python does not need DSLtk or the C++20 DSL to use this package.

## Build from the complete repository

Requires CMake 3.18+, Python interpreter **and development headers**, a C11/C++17
compiler, and the normal MetaArch codec dependencies. nanobind is a build-time
dependency, not a Python runtime dependency. Its checkout must include
`ext/robin_map/include` (clone recursively).

```sh
cmake -S . -B build-python -DCMAKE_BUILD_TYPE=Release \
  -DARCHDB_BUILD_DSL=OFF -DARCHDB_TEST_PYTHON_BINDINGS=ON \
  -DARCHDB_NANOBIND_DIR=/mnt/nvme/metatk/third_party/nanobind
cmake --build build-python -j
ctest --test-dir build-python --output-on-failure
PYTHONPATH="$PWD/build-python/bindings/python" python3 -c \
  'import metaarch; print(metaarch.abi_version())'
```

Use the actual checkout location on your machine. The originally requested
`/mnt/nvme/third_party/nanobind` was absent in the development environment; the
checkout above was used instead. No absolute checkout path is hard-coded in the
build. With `ARCHDB_NANOBIND_DIR` unset, CMake searches for an installed nanobind
CMake package, then for `python -m nanobind --cmake_dir`. nanobind is not fetched
automatically. Select an interpreter with `-DPython_EXECUTABLE=/path/to/python`.

`ARCHDB_BUILD_PYTHON=ON` builds without enabling tests. Both Python options
default off (the test option follows `ARCHDB_TEST_FOREIGN_BINDINGS`). Enabling
tests implies building the extension. The C-only default remains independent of
Python and nanobind. For offline yyjson builds, add
`-DFETCHCONTENT_SOURCE_DIR_YYJSON=/path/to/yyjson`.

### Installation

```sh
cmake --install build-python --prefix /your/prefix
# Substitute your Python minor version and lib/lib64 directory:
export PYTHONPATH=/your/prefix/lib/python3.14/site-packages
```

The extension, wrapper, type stubs and `py.typed` marker install together.
Override `ARCHDB_PYTHON_INSTALL_DIR` for a different package directory. Shared
builds use a relative install RPATH matching the default directory; if relocating
the package independently, configure the loader path to `libmetaarch` yourself.
The package does not bundle architecture specs; normal CMake install places them
under `share/metaarch/ArchDB`.

A platform-specific wheel can also be built **from this complete checkout**:

```sh
python3 -m pip wheel ./bindings/python -w dist \
  -Ccmake.define.ARCHDB_NANOBIND_DIR=/mnt/nvme/metatk/third_party/nanobind
python3 -m pip install dist/metaarch-*.whl
```

This optional route requires `scikit-build-core` (pip build isolation installs
it). `--no-build-isolation` works when it is already installed. Supply yyjson's
source path through another `-Ccmake.define.FETCHCONTENT_SOURCE_DIR_YYJSON=...`
when offline. Wheels use static MetaArch and contain only the Python component,
not the database or CLI. System codec libraries can remain dynamic dependencies;
these are local-platform wheels, **not audited manylinux wheels**. Copying only
`bindings/python` or publishing its sdist is not supported: the native sources
and root CMake files are required from the enclosing repository.

## Quick start

```python
import metaarch as ma

with ma.load("ArchDB/riscv64.dsymspec.yaml") as spec:
    a0 = spec.register("a0")
    result = spec.encode("addi", {"rd": a0.encoding, "rs1": a0.encoding, "imm": -1})
    print(result.data.hex(" "))  # 13 05 f5 ff
    text = spec.write(ma.Format.JSON, ma.WriteFlags.REF_BACKEND)

# Result bytes and metadata own their storage and survive spec.close().
print(result.data)
```

## API reference

The package exports `Spec`, `Diagnostic`, `Target`, `Register`, `Form`,
`Encoded`, `Relocation`, `Error`, the enums below, `load`, `parse` and
`abi_version()` (currently 1). `metaarch/__init__.pyi` contains typed signatures.
Metadata/result classes are returned by the library, not user-constructible;
their attributes are read-only. Returned lists are independent copies.
Absent optional metadata strings are represented as empty strings.

| Operation | Contract |
|---|---|
| `load(path, format=Format.AUTO, flags=0)` | `str` or text `PathLike`; detect format by file extension |
| `parse(data, format, path="<memory>", flags=0)` | `str` (UTF-8 encoded) or `bytes`; explicit format; diagnostic/base path |
| `Spec.load(...)`, `Spec.parse(...)` | Same native operations; paths must be `str` |
| `spec.close()` | Idempotent deterministic cleanup; context manager recommended |
| `spec.diagnostics` | Copied load-time warnings/notes, also available after close |
| `spec.target_count`, `register_count`, `encoder_count`, `form_count` | Record counts |
| `spec.target(i)` | `Target`: arch, subarch, object_format, abi, address_width, byte_order |
| `spec.register(i_or_name)` | `Register`: name, reg_class, width, encoding |
| `spec.form(i)` | `Form`: mnemonic, encoder, operand_count, visible_operand_count |
| `spec.encoder_name(i)` | Encoder name |
| `spec.find_encoder(name)`, `find_register(name)` | Index or -1; case-sensitive; Python register-index lookup scans the table |
| `spec.load_backend()` | Explicit mutation; resolves file relative to spec's path |
| `spec.write(format, flags=0)` | Owned Unicode text in any of the four formats |
| `spec.encode(encoder, operands, target=0)` | Low-level named encoding; `operands` is a `dict[str, int]` |
| `spec.encode_form(form, values, target=0)` | Checked form encoding; positional integer sequence |

Indices are zero-based, nonnegative. Named encoder arguments must bind every
parameter, with no extras (at most 64 bindings). Values must be genuine Python
integers in `[-2**63, 2**63-1]`; bools, floats, register-name strings and oversized
integers are rejected. Resolve names with `spec.register(name).encoding` first.
Form encoding skips hidden/implicit records and maps visible operands to encoder
parameters in first-use order. It enforces declared register class/width,
immediate bounds and relocation mapping. It does **not** parse memory syntax,
constraint strings or target feature gates. See [encoder rules](../../manual/encoding.md).

`Encoded.data` is `bytes`; `Encoded.relocations` is a copied list. Relocation
attributes: `name`, `offset`, `size`, `bit_offset`, `bit_width`, `byte_order`,
`symbol_parameter`, `symbol_value`. Byte offset/size locate the emitted field;
bit offset is relative to its integer LSB, not the first byte in memory. These
records do not perform linking or symbol resolution.

### Enums and flags

| Type | Members |
|---|---|
| `Format` | `AUTO`, `YAML`, `JSON`, `XML`, `SEXPR` (AUTO for file load only) |
| `LoadFlags` | `NONE`, `BACKEND`, `NO_COMPILE`, `STRICT`, `LINT` |
| `WriteFlags` | `NONE`, `COMPACT`, `EMBED_BACKEND`, `REF_BACKEND`, `NO_HEADER`, `SORT_KEYS` |
| `Severity` | `NOTE`, `WARNING`, `ERROR` |
| `ByteOrder` | `LITTLE`, `BIG` |

Combine flags with `|`. `SORT_KEYS` is a generic-tree flag; it is not a promise
to reorder typed model records. JSON embeds an already loaded backend by
default; use `REF_BACKEND` to retain a file reference. Writers never implicitly
load a missing backend. Backend paths must follow the schema's
`backend/<name>.json` convention.

### Errors, ownership and threads

```python
try:
    spec = ma.parse("schema: 1\ntargets: wrong\n", ma.Format.YAML, path="bad.yaml")
except ma.Error as error:
    for d in error.diagnostics:
        print(d.file, d.line, d.column, d.path, d.severity, d.message)
```

Native operation failures raise `Error(RuntimeError)` with owned diagnostics.
Invalid metadata indices/missing register names raise `IndexError`, embedded NUL
names/paths raise `ValueError`, operand overflow raises `OverflowError`, wrong
types raise `TypeError`, and using a closed spec raises `RuntimeError`. Parser
line/column accuracy depends on the codec (JSON binding errors can lack positions).
No C pointer or layout is exposed. Native destructors release specs at Python
object destruction as a safety net; prefer `with` or `close()` for prompt cleanup.

Calls retain the Python GIL, including native work, so reads, close and backend
loading cannot overlap within the supported GIL-enabled interpreter. This is
safe but does not offer parallel native throughput. No free-threaded/Python
subinterpreter support is claimed. Catalogue access and the optional C++20 DSL
and assembler are **not** wrapped; use the native APIs/CLI for those.

## Tests

```sh
PYTHONPATH="$PWD/build-python/bindings/python" \
  python3 -m unittest discover -s bindings/python/tests -v
```

The 24 unittest cases use `tests/fixtures/api.yaml`, temporary backend files,
and all 28 shipped specs; they require no Python test dependencies. Run from the
repository root or set `METAARCH_SOURCE_DIR`. For installation/sanitizer checks,
see [verification](../../manual/verification.md).
