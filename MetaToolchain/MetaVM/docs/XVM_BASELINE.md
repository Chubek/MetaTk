# XVM baseline — 2026-11-05

Observed before changing Machinery, Linux amd64:

| Configuration | Result | Wall time |
| --- | --- | ---: |
| MetaVM GCC Release, C++ wrapper enabled | 22/22 CTest cases; original Machinery 5,056 checks | 2.32 s |
| Standalone Machinery Clang Debug ASan/UBSan | 4/4 CTest cases | 0.06 s |
| DSLtk/MetaML C++ and D libraries | 26/26 CTest cases | 10.88 s |

Machinery used its existing `-Wall -Wextra -Wpedantic -Werror` policy. No warning
was observed in these baseline builds. Timings are smoke-test wall times on a
shared machine, not performance budgets.

```sh
cmake -S MetaToolchain/MetaVM -B /tmp/opencode/metavm-xvm-baseline \
  -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON
cmake --build /tmp/opencode/metavm-xvm-baseline -j4
ctest --test-dir /tmp/opencode/metavm-xvm-baseline --output-on-failure -j4

cmake -S MetaToolchain/MetaVM/Machinery \
  -B /tmp/opencode/metavm-xvm-baseline-sanitize \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ -DMETAVM_TEST_CPP=ON -DMETAVM_SANITIZE=ON
cmake --build /tmp/opencode/metavm-xvm-baseline-sanitize -j4
ctest --test-dir /tmp/opencode/metavm-xvm-baseline-sanitize --output-on-failure
```

Scope: XVM only. Architecture specs, generated backend data and preserved
third-party dependency bytes are unchanged.
