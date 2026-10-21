# Binding tests

Run from `dombind/` using a Python environment where the repository's `q` package
is importable:

```sh
python -m unittest discover -s tests -v
```

No pytest dependency is needed. Each test generates headers into a temporary
directory and cleans its build outputs. Native Rust tests use Cargo offline.

Required tools depend on the test: C/C++ compilers, pkg-config, Lua 5.4, Ruby,
Node with Node-API headers, a JDK (JAVA_HOME), OCaml, Go with cgo, Rust/Cargo,
and LDC. Missing SDKs are explicit skips; compiler errors and runtime failures
are failures. `NODE_INCLUDE_DIR` can override Node header discovery. Make sure
`GOROOT` identifies the actual Go installation. `DOMBIND_GO_VET=off` explicitly
disables vet for a distribution that omits it; the default runs Go's normal vet
checks.

Coverage:

- Every module, duplicate inclusion, and two prefix instantiations in C/C++.
- C++ ownership-header compilation for every available runtime.
- Lua stack restoration after errors, references surviving state-wrapper
  destruction, table access and length-preserving binary strings.
- Ruby GC roots, array/string access and exception recovery. Tests explicitly
  enable assertions after Ruby headers, which can define NDEBUG.
- Node module loading, reference copies, binary strings and C++ error translation.
- JNI local/global references, UTF-16 lengths, method calls and pending exceptions.
- OCaml rooted values through compaction and encoded callback exceptions.
- Buffer deep-copy/move/free, invalid inputs, overflow-safe slices and callbacks.
- Standalone generator CLIs and separate consumers of installed CMake packages.
- Native Go/Rust/D unit tests and shared exports called through ctypes, exercising
  exact pointer/size/scalar ABI behavior and null-input rejection.

The tests intentionally do not replace missing SDKs with fake runtime headers.
