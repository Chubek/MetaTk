# Verification (2026-10-30)

## Hardening pass

API addition: `metapp_preprocess_n(ctx, source, length, filename, output)`.
Existing entry points remain available. The C++ wrapper now uses the
length-delimited API and rejects embedded NULs in configuration arguments.

Before edits, the original four CTest cases passed under GCC Release in 0.87 s.
Independent GCC comparisons reproduced lost digraph spellings, rejected valid
digraph pastes, lost whitespace after empty expansions, and acceptance of
invalid mixed-case `lL` integer suffixes. All now have regression coverage.

Additional hardening:

* Token pasting and `_Pragma` re-lexing no longer repeat trigraph conversion or
  backslash-newline splicing from the source translation phases.
* stdin, file and C++ binary inputs reject embedded NULs; oversize buffers are
  rejected before copying. CLI failure leaves existing output files intact.
* Lexing and substitution check token limits before growing containers.
  Cumulative substitution text and processed-file counts are bounded.
  Rendering and literal quoting check output size before appending.
* Empty `#define`, `#if`, `#elif` and `#include` diagnostics retain location.
* Control characters in `__FILE__` use unambiguous octal escapes; identifier
  classification and date/time formatting do not depend on the active locale.
* Regression checks cover recovery after include-depth errors and budget
  boundaries, including a non-NUL-terminated C input buffer.

The suite now contains **82 unit checks**, a C ABI test, CLI tests, **512**
fixed-seed byte/macro robustness inputs, **189** successful independent compiler
comparisons, and **5** independently rejected malformed-suffix cases.

All configurations use `-Wall -Wextra -Wpedantic -Werror` for both languages:

| Configuration | CTest result | Elapsed |
| --- | --- | --- |
| GCC 16.2.1 Release | 5/5 passed | 1.64 s |
| Clang 22.1.8 Release | 5/5 passed | 2.77 s |
| GCC Debug, ASan + UBSan | 5/5 passed | 6.71 s |
| Clang Debug, ASan + UBSan | 5/5 passed | 6.96 s |

No compiler warnings or sanitizer findings were reported. These deterministic
robustness checks are not an exhaustive fuzz campaign or ISO conformance suite.
The language limitations in README.md still apply.

## Initial implementation

The starting MetaPP scaffold contained only empty files and had no build or
tests. The new standalone suite has four CTest cases: unit regressions, the
C API, command-line behavior, and 182 independent GCC/Clang comparisons.

All configurations used `-Wall -Wextra -Wpedantic -Werror` for C and C++:

| Configuration | CTest result | Elapsed |
| --- | --- | --- |
| GCC 16.2.1 Release | 4/4 passed | 1.05 s |
| Clang 22.1.8 Release | 4/4 passed | 2.11 s |
| GCC Debug, ASan + UBSan | 4/4 passed | 2.40 s |
| Clang Debug, ASan + UBSan | 4/4 passed | 3.39 s |

No compiler warnings or sanitizer findings were reported. Unix Makefiles were
used because Ninja is not installed in this environment. See README.md for
build/test commands and the implemented language subset.

Installation and the exported package can be checked independently:

```sh
cmake --install build/metapp --prefix /tmp/opencode/metapp-install
cmake -S MetaPP/tests/consumer -B build/metapp-consumer \
  -DCMAKE_PREFIX_PATH=/tmp/opencode/metapp-install
cmake --build build/metapp-consumer
ctest --test-dir build/metapp-consumer --output-on-failure
```
