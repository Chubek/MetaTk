# 2. Building and testing

Use a C++20 compiler, CMake 3.24 or newer, and Python 3. A D compiler is optional for C++ use, but required to verify dialect parity. The test build discovers LDC, DMD, or GDC. A C compiler is also used: the C API regression client is deliberately compiled as C99, not disguised C++.

```sh
cmake -S tests -B build -DDSLtk_TEST_D=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
python3 scripts/parity_check.py --cpp build/metaml-repl-cpp --d build/metaml-repl-d --trace
```

This standalone configuration builds the C++ and available D REPLs, runtime regression tests, the `metaml-c` static adapter library, and the existing DSLtk tests. The top-level CMake project can also build the tests. Turn off `DSLtk_TEST_D` when intentionally working without a D compiler; that skips D coverage, not proof of parity.

For a minimal interpreter without the optional host bundle, a C++ program needs only the headers and the usual platform threading linkage. The supplied REPL detects `Metalib.h` and installs the portable host bundle. To compile the D REPL directly with LDC, use automatic import compilation:

```sh
ldc2 -i -I. repl/metaml.d -of=metaml-d
```

The files named `metalib/cpp/X.c` are C++ implementation fragments included by `X.h`. Never pass them to a C compiler or compile them independently. The genuine C interface is `metalib/c/MetaML.h`; its implementation is `MetaML.cpp` and must be linked with a C++ linker/runtime.

**Check:** run a library fixture with `build/metaml-repl-cpp --stdlib metalib metalib/list/test.ml`. A successful run exits zero and contains no failed assertions.

<!-- nav:begin -->
[Contents](README.md) · [Previous](01-introduction.md) · [Next](03-repl.md)
<!-- nav:end -->
