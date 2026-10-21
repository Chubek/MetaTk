# Python

The complete [Python build and API reference](../bindings/python/README.md)
documents every exported operation, enum, ownership rule and exception. Typed
signatures are in `bindings/python/metaarch/__init__.pyi`.

## Build and import

```sh
cmake -S . -B build-python -DARCHDB_BUILD_DSL=OFF \
  -DARCHDB_TEST_PYTHON_BINDINGS=ON \
  -DARCHDB_NANOBIND_DIR=/mnt/nvme/metatk/third_party/nanobind
cmake --build build-python -j
ctest --test-dir build-python -R metaarch_python_api --output-on-failure
export PYTHONPATH="$PWD/build-python/bindings/python"
```

The checkout path is configurable. Python development headers and a C++17
compiler are required to build. The package uses `metaarch._native`, not ctypes,
so putting **source** `bindings/python` on PYTHONPATH is insufficient: use the
build or installed package containing the compiled extension.

## Inspect and encode

```python
from pathlib import Path
import metaarch as ma

with ma.load(Path("ArchDB/riscv64.dsymspec.yaml")) as spec:
    print(spec.target(0).arch)
    for i in range(spec.register_count):
        register = spec.register(i)
        print(register.name, register.reg_class, register.width, register.encoding)

    value = spec.register("a0").encoding
    encoded = spec.encode("addi", {"rd": value, "rs1": value, "imm": -1})
    print(encoded.data.hex(" "))  # 13 05 f5 ff
    json_text = spec.write(ma.Format.JSON, ma.WriteFlags.REF_BACKEND)

with ma.parse(json_text, ma.Format.JSON) as reparsed:
    print(reparsed.encoder_count)
```

For a form with known integer operands, use `spec.encode_form(index, values)`.
That checks declared operand bounds and register metadata. Never assume form
list order equals encoder list order. Complex-memory mappings can legitimately
fail rather than being guessed. The API accepts integers; it is not an assembler.

## Errors and lifetime

```python
try:
    with ma.load("bad.yaml", flags=ma.LoadFlags.LINT) as spec:
        print(spec.diagnostics)
except ma.Error as error:
    for diagnostic in error.diagnostics:
        print(diagnostic.file, diagnostic.line, diagnostic.column,
              diagnostic.path, diagnostic.message)
```

All returned metadata, diagnostics, bytes and relocation records own their
storage, so they can outlive a spec. Context managers close on normal or
exceptional exits without swallowing exceptions. `close()` is idempotent.
Other native operations on closed handles fail. Calls retain the GIL: safe
serialized access, not concurrent native execution. No free-threaded or
subinterpreter support is claimed.

Load backends explicitly before writing embedded data; loading is a mutation.
Use REF_BACKEND when a portable reference rather than a large payload is wanted.
No native catalogue, `.march` parser or text assembler is exposed in Python v1.
