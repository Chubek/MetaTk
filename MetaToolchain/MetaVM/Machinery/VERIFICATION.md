# Machinery verification — 2026-10-30

Baseline: the supplied Machinery directory contained 28 empty files and no
build or tests. The implemented interface is the new `mvm_*` C API plus the
optional `metavm` C++11 ownership wrapper; see the public headers and README.

Verified on Linux with GCC/G++ 16.2.1 and Clang/Clang++ 22.1.8:

| Build | Result |
|---|---|
| GCC Release, C11 + optional C++11 tests | 4/4 CTest cases passed |
| Clang Release, C11 + optional C++11 tests | 4/4 passed |
| GCC Debug, ASan + UBSan | 4/4 passed |
| Clang Debug, ASan + UBSan | 4/4 passed |
| C-only shared-library build through `MetaVM/CMakeLists.txt` | 3/3 passed |
| Installed CMake package, separate C-only consumer | 1/1 passed |

The C test performs **5,056 checks**, including 2,000 deterministic binary
mutations and 1,000 generated assembly inputs. Both engine examples print
`3628800`. Release CTest runs took approximately 0.01 s; sanitizer runs 0.07 s
on this machine. These are smoke-test timings, not performance budgets.

All builds use `-Wall -Wextra -Wpedantic -Werror`; language extensions are off.
Every public C header was also compiled independently as C11 and C++11 by both
compilers with `-pedantic-errors`. Clang static analysis of all eight library
translation units is clean. `readelf -d libmetavm.so` reports only `libc.so.6`
as a needed library. There are no downloads or third-party link dependencies.

The build/test commands are in README.md. Unix Makefiles were used here because
Ninja is not installed. Verification is scoped to Machinery and its standalone
parent build, not the unrelated MetaArch or legacy MetaJIT test suites.

Platform/runtime boundaries: the full MetaIL language and the legacy MetaJIT
C++ Program/Frame API are not implemented by this component. The MetaIL headers
provide dependency-free scalar operation selection/evaluation. Native callbacks
are explicit C functions, with no dynamic library loader or arbitrary-ABI FFI.
Windows and non-Linux execution were not tested.
